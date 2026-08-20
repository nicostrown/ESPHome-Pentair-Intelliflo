#include "intelliflo.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace pentair_intelliflo {

static const char *const TAG = "pentair_intelliflo";

// We impersonate the pool controller.
static const uint8_t CONTROLLER_ADDRESS = 0x10;

// The pump is slow to turn the line around. Leave a gap between our frames.
static const uint32_t TX_SPACING_MS = 200;
// Give up on a half-received frame after this long.
static const uint32_t RX_IDLE_TIMEOUT_MS = 150;
// Don't start transmitting if a frame arrived within this window.
static const uint32_t RX_QUIET_MS = 20;

static const uint8_t MAX_PAYLOAD_LEN = 32;
static const size_t MAX_TX_QUEUE = 12;
static const uint16_t MAX_RPM = 3450;

static std::string program_to_string(uint8_t value) {
  switch (value) {
    case 0x00:
      return "Filter";
    case 0x01:
      return "Manual";
    case 0x02:
      return "Backwash";
    case 0x03:
      return "Speed 3";
    case 0x04:
      return "Speed 4";
    case 0x06:
      return "Feature 1";
    case 0x09:
      return "External Program 1";
    case 0x0A:
      return "External Program 2";
    case 0x0B:
      return "External Program 3";
    case 0x0C:
      return "External Program 4";
    case 0x0D:
      return "Quick Clean";
    case 0x0E:
      return "Timeout";
    case 0x11:
      return "Priming";
    default:
      return str_snprintf("Unknown (0x%02X)", 20, value);
  }
}

static std::string pump_state_to_string(uint8_t value) {
  switch (value) {
    case 0x00:
      return "Fault";
    case 0x01:
      return "Priming";
    case 0x02:
      return "Running";
    case 0x04:
      return "System Priming";
    default:
      return str_snprintf("Unknown (0x%02X)", 20, value);
  }
}

static std::string error_to_string(uint8_t value) {
  switch (value) {
    case 0x00:
      return "OK";
    case 0x02:
      return "Filter error";
    default:
      return str_snprintf("Error 0x%02X", 16, value);
  }
}

void PentairIntelliflo::setup() { this->rx_.reserve(64); }

void PentairIntelliflo::dump_config() {
  ESP_LOGCONFIG(TAG, "Pentair IntelliFlo:");
  ESP_LOGCONFIG(TAG, "  Pump address: 0x%02X", this->address_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Power", this->power_);
  LOG_SENSOR("  ", "Speed", this->rpm_);
  LOG_SENSOR("  ", "Flow", this->flow_);
  LOG_SENSOR("  ", "Filter percent", this->filter_percent_);
  LOG_SENSOR("  ", "Error code", this->error_code_);
  LOG_SENSOR("  ", "Time remaining", this->time_remaining_);
  LOG_BINARY_SENSOR("  ", "Running", this->running_);
  LOG_BINARY_SENSOR("  ", "Remote control", this->remote_control_);
  LOG_TEXT_SENSOR("  ", "Program", this->program_);
  LOG_TEXT_SENSOR("  ", "Pump state", this->pump_state_);
  LOG_TEXT_SENSOR("  ", "Error", this->error_);
}

void PentairIntelliflo::update() { this->request_status(); }

void PentairIntelliflo::loop() {
  const uint32_t now = millis();

  if (!this->rx_.empty() && now - this->last_rx_ms_ > RX_IDLE_TIMEOUT_MS) {
    ESP_LOGV(TAG, "Dropping %u stale RX bytes", (unsigned) this->rx_.size());
    this->rx_.clear();
  }

  uint8_t byte;
  while (this->available() > 0 && this->read_byte(&byte)) {
    this->last_rx_ms_ = now;
    this->feed_byte_(byte);
  }

  if (this->tx_queue_.empty() || now - this->last_tx_ms_ < TX_SPACING_MS)
    return;
  // Never talk over an inbound frame.
  if (now - this->last_rx_ms_ < RX_QUIET_MS)
    return;

  const std::vector<uint8_t> frame = std::move(this->tx_queue_.front());
  this->tx_queue_.pop_front();
  this->write_array(frame.data(), frame.size());
  this->flush();
  this->last_tx_ms_ = millis();
  ESP_LOGD(TAG, "TX: %s", format_hex_pretty(frame).c_str());
}

// Frames look like: [FF...] 00 FF A5 <proto> <dst> <src> <cmd> <len> <payload> <ckHi> <ckLo>
// The leading 0xFF run is line idle/wake-up padding and its length varies, so we
// sync on the "00 FF A5" sequence and only ever discard one byte at a time when
// something does not line up. Discarding the whole buffer (as the upstream
// component does) throws away the frame that follows a bad byte.
void PentairIntelliflo::feed_byte_(uint8_t byte) {
  this->rx_.push_back(byte);

  while (true) {
    if (!this->resync_())
      return;
    if (this->rx_.size() < 8)
      return;  // header + length byte not complete yet

    const uint8_t len = this->rx_[7];
    if (len > MAX_PAYLOAD_LEN) {
      ESP_LOGW(TAG, "Implausible payload length %u, resyncing", len);
      this->rx_.erase(this->rx_.begin());
      continue;
    }

    const size_t total = (size_t) len + 10;
    if (this->rx_.size() < total)
      return;

    // Checksum covers 0xA5 through the last payload byte.
    uint16_t sum = 0;
    for (size_t i = 2; i < (size_t) 8 + len; i++)
      sum += this->rx_[i];
    const uint16_t expected = ((uint16_t) this->rx_[8 + len] << 8) | this->rx_[9 + len];

    if (sum != expected) {
      ESP_LOGW(TAG, "Checksum mismatch (calc 0x%04X, frame 0x%04X): %s", sum, expected,
               format_hex_pretty(this->rx_.data(), total).c_str());
      this->rx_.erase(this->rx_.begin());
      continue;
    }

    this->handle_frame_(total);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + total);
  }
}

bool PentairIntelliflo::resync_() {
  while (!this->rx_.empty()) {
    const size_t n = this->rx_.size();
    if (this->rx_[0] != 0x00) {
      this->rx_.erase(this->rx_.begin());
      continue;
    }
    if (n >= 2 && this->rx_[1] != 0xFF) {
      this->rx_.erase(this->rx_.begin());
      continue;
    }
    if (n >= 3 && this->rx_[2] != 0xA5) {
      this->rx_.erase(this->rx_.begin());
      continue;
    }
    return true;
  }
  return false;
}

void PentairIntelliflo::handle_frame_(size_t total) {
  const uint8_t src = this->rx_[5];
  const uint8_t cmd = this->rx_[6];
  const uint8_t len = this->rx_[7];
  const uint8_t *payload = this->rx_.data() + 8;

  ESP_LOGV(TAG, "RX: %s", format_hex_pretty(this->rx_.data(), total).c_str());

  // Half-duplex means we usually hear our own frames (src 0x10) as well as
  // anything else on the bus. Only our pump's replies are interesting.
  if (src != this->address_)
    return;

  switch (cmd) {
    case 0x04:  // remote control acknowledgement
      if (len >= 1) {
        const bool remote = payload[0] != 0x00;
        if (remote != this->remote_state_)
          ESP_LOGI(TAG, "Pump switched to %s control", remote ? "REMOTE" : "LOCAL");
        this->remote_state_ = remote;
        if (this->remote_control_ != nullptr)
          this->remote_control_->publish_state(remote);
      }
      break;

    case 0x07:  // status
      if (len >= 15) {
        this->publish_status_(payload);
      } else {
        ESP_LOGW(TAG, "Status frame too short (%u payload bytes)", len);
      }
      break;

    default:
      ESP_LOGD(TAG, "Pump acked command 0x%02X: %s", cmd, format_hex_pretty(payload, len).c_str());
      break;
  }
}

void PentairIntelliflo::publish_status_(const uint8_t *p) {
  const bool running = p[0] == PUMP_STARTED;
  const uint16_t watts = ((uint16_t) p[3] << 8) | p[4];
  const uint16_t rpm = ((uint16_t) p[5] << 8) | p[6];

  this->running_state_ = running;
  this->watts_state_ = watts;
  this->rpm_state_ = rpm;

  if (this->running_ != nullptr)
    this->running_->publish_state(running);
  if (this->program_ != nullptr)
    this->program_->publish_state(program_to_string(p[1]));
  if (this->pump_state_ != nullptr)
    this->pump_state_->publish_state(pump_state_to_string(p[2]));
  if (this->power_ != nullptr)
    this->power_->publish_state(watts);
  if (this->rpm_ != nullptr)
    this->rpm_->publish_state(rpm);
  if (this->flow_ != nullptr)
    this->flow_->publish_state(p[7]);
  if (this->filter_percent_ != nullptr)
    this->filter_percent_->publish_state(p[8]);
  if (this->error_code_ != nullptr)
    this->error_code_->publish_state(p[10]);
  if (this->error_ != nullptr)
    this->error_->publish_state(error_to_string(p[10]));
  if (this->time_remaining_ != nullptr)
    this->time_remaining_->publish_state(p[11] * 60 + p[12]);

  ESP_LOGI(TAG, "Status: %s, %u rpm, %u W, %u gpm, mode 0x%02X, drive 0x%02X, error 0x%02X",
           running ? "started" : "stopped", rpm, watts, p[7], p[1], p[2], p[10]);
}

void PentairIntelliflo::queue_command_(uint8_t command, const std::vector<uint8_t> &payload) {
  if (this->tx_queue_.size() >= MAX_TX_QUEUE) {
    ESP_LOGW(TAG, "TX queue full, dropping command 0x%02X", command);
    return;
  }

  // Everything from the 0xA5 up to the last payload byte is checksummed.
  std::vector<uint8_t> body;
  body.reserve(6 + payload.size());
  body.push_back(0xA5);
  body.push_back(0x00);  // protocol 0x00: pump messages
  body.push_back(this->address_);
  body.push_back(CONTROLLER_ADDRESS);
  body.push_back(command);
  body.push_back((uint8_t) payload.size());
  body.insert(body.end(), payload.begin(), payload.end());

  uint16_t sum = 0;
  for (uint8_t b : body)
    sum += b;

  std::vector<uint8_t> frame;
  frame.reserve(body.size() + 5);
  frame.push_back(0xFF);  // idle padding so the pump's UART wakes up
  frame.push_back(0x00);
  frame.push_back(0xFF);
  frame.insert(frame.end(), body.begin(), body.end());
  frame.push_back((uint8_t) (sum >> 8));
  frame.push_back((uint8_t) (sum & 0xFF));

  this->tx_queue_.push_back(std::move(frame));
}

void PentairIntelliflo::request_status() { this->queue_command_(0x07, {}); }

void PentairIntelliflo::set_remote_control(bool remote) {
  this->queue_command_(0x04, {(uint8_t) (remote ? 0xFF : 0x00)});
}

void PentairIntelliflo::set_pump_running(bool running) {
  this->queue_command_(0x06, {running ? PUMP_STARTED : PUMP_STOPPED});
}

void PentairIntelliflo::set_speed_rpm(uint16_t rpm) {
  rpm = std::min(rpm, MAX_RPM);
  ESP_LOGD(TAG, "Setting speed to %u rpm", rpm);
  this->queue_command_(0x01, {0x02, 0xC4, (uint8_t) (rpm >> 8), (uint8_t) (rpm & 0xFF)});
}

void PentairIntelliflo::set_speed_gpm(uint8_t gpm) {
  ESP_LOGD(TAG, "Setting flow to %u gpm", gpm);
  this->queue_command_(0x01, {0x02, 0xE4, 0x00, gpm});
}

void PentairIntelliflo::set_program_speed(uint8_t program, uint16_t rpm) {
  if (program < 1 || program > 4) {
    ESP_LOGW(TAG, "Program must be 1-4, got %u", program);
    return;
  }
  rpm = std::min(rpm, MAX_RPM);
  ESP_LOGD(TAG, "Storing %u rpm in external program %u", rpm, program);
  this->queue_command_(0x01, {0x03, (uint8_t) (0x26 + program), (uint8_t) (rpm >> 8),
                              (uint8_t) (rpm & 0xFF)});
}

void PentairIntelliflo::run_program(uint8_t program) {
  if (program > 4) {
    ESP_LOGW(TAG, "Program must be 0-4, got %u", program);
    return;
  }
  ESP_LOGD(TAG, "Activating external program %u", program);
  this->queue_command_(0x01, {0x03, 0x21, 0x00, (uint8_t) (program * 8)});
}

void PentairIntelliflo::set_speed_index(uint8_t index) {
  ESP_LOGD(TAG, "Selecting built-in speed 0x%02X", index);
  this->queue_command_(0x05, {index});
}

}  // namespace pentair_intelliflo
}  // namespace esphome
