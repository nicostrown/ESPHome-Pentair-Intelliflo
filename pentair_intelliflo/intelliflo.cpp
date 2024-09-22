#include "intelliflo.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace intelliflo {

static const char *const TAG = "intelliflo";

void Intelliflo::setup() {}

void Intelliflo::loop() {
  while (this->available() > 0) {
    uint8_t c;
    this->read_byte(&c);
    this->last_received_byte_millis = millis();
    this->handle_received_byte(c);
  }

  // allow sending only after 100ms since last received message
  if (millis() - this->last_received_byte_millis > 100) {
    this->ready_to_tx = true;
  }

  if (this->ready_to_tx && !this->tx_buffer.empty()) {
    // send the first command in the queue
    this->send_array_cmd(this->tx_buffer.front());
    this->tx_buffer.pop();
    this->last_received_byte_millis = millis();
    this->ready_to_tx = false;
  }
}

void Intelliflo::dump_config() { ESP_LOGCONFIG(TAG, "  Pentair Intelliflo"); }

void Intelliflo::handle_received_byte(uint8_t c) {
  this->rx_buffer.push_back(c);
  if (!this->validate_received_message()) {
    this->rx_buffer.clear();
  }
}

bool Intelliflo::validate_received_message() {
  uint32_t at = this->rx_buffer.size() - 1;  // position of the last received byte
  uint8_t *data = &this->rx_buffer[0];       // pointer to the first byte of the message

  // Byte 0: HEADER (Always 0xFF)
  if (at == 0) {
    return data[0] == 0xFF;
  }
  // Byte 1: HEADER (Always 0x00)
  if (at == 1) {
    return data[1] == 0x00;
  }
  // Byte 2: HEADER (Always 0xFF)
  if (at == 2) {
    return data[2] == 0xFF;
  }
  // Byte 3: HEADER (Always 0xA5)
  if (at == 3) {
    return data[3] == 0xA5;
  }

  // Byte 2: packet_size - number of bytes further + 1
  // Check is not carried out
  if (at <= 8) {
    return true;
  }
  uint8_t packet_size = data[8];
  uint8_t length = (packet_size + 10);

  // wait until all data of the package arrives
  if (at < length) {
    return true;
  }

  // calculate the checksum
  uint16_t checksum = 0;
  for (int j = 3; j < 3 + packet_size + 6; j++) {
    checksum = checksum + data[j];
  }

  uint16_t packet_checksum = (data[3 + 6 + packet_size] << 8) + data[3 + 7 + packet_size];
  if (checksum != packet_checksum) {
    ESP_LOGW(TAG, "CHECKSUM MISMATCH - got %d, expected %d", checksum, packet_checksum);
    return false;
  }

  // the correct message was received and lies in the rx_buffer buffer

  // Remove 0xFF 00 FF from the beginning of the message
  rx_buffer.erase(rx_buffer.begin());
  rx_buffer.erase(rx_buffer.begin());
  rx_buffer.erase(rx_buffer.begin());

  std::string pretty_cmd = format_hex_pretty(rx_buffer);
  ESP_LOGI(TAG, "Package received: %S ", pretty_cmd.c_str());

  parse_packet(rx_buffer);

  // return false to reset rx buffer
  return false;
}

void Intelliflo::parse_packet(const std::vector<uint8_t> &data) {
  if (data[3] == 0x60 && data[4] == 0x01) {
    ESP_LOGI(TAG, "Pump goes to ext program %d", data[6]);
  } else if (data[3] == 0x60 && data[4] == 0x04 && data[6] == 0x00) {
    ESP_LOGI(TAG, "Pump is local");
  } else if (data[3] == 0x60 && data[4] == 0x04 && data[6] == 0xFF) {
    ESP_LOGI(TAG, "Pump is remote");
  } else if (data[3] == 0x60 && data[4] == 0x05) {
    ESP_LOGI(TAG, "Pump goes to local program %02x", data[7]);
  } else if (data[3] == 0x60 && data[4] == 0x07) {
    // we have a pump status packet

    if (this->running_ != nullptr)
      switch (data[6]) {
        case STOPPED:
          this->running_->publish_state(false);
          break;
        case RUNNING:
          this->running_->publish_state(true);
          break;
        default:
          ESP_LOGW(TAG, "Received unknown running value %0x02x", data[6]);
          break;
      }

    if (this->program_ != nullptr)
      switch (data[7]) {
        case NO_PROG:
          this->program_->publish_state("");
          break;
        case LOCAL1:
          this->program_->publish_state("Local 1");
          break;
        case LOCAL2:
          this->program_->publish_state("Local 2");
          break;
        case LOCAL3:
          this->program_->publish_state("Local 3");
          break;
        case LOCAL4:
          this->program_->publish_state("Local 4");
          break;
        case EXT1:
          this->program_->publish_state("External 1");
          break;
        case EXT2:
          this->program_->publish_state("External 2");
          break;
        case EXT3:
          this->program_->publish_state("External 3");
          break;
        case EXT4:
          this->program_->publish_state("External 4");
          break;
        case TIMEOUT:
          this->program_->publish_state("Time Out");
          break;
        case PRIMING:
          this->program_->publish_state("Priming");
          break;
        case QUICKCLEAN:
          this->program_->publish_state("Quick Clean");
          break;
        case UNKNOWN:
          this->program_->publish_state("Unknown");
          break;
        default:
          ESP_LOGW(TAG, "Received unknown program value %0x02x", data[7]);
          break;
      }

    // this->status = packet.data[2];   // 0x01=Priming 0x02=Running 0xFF=?
    if (this->power_ != nullptr)
      this->power_->publish_state((data[9] * 256) + data[10]);
    if (this->rpm_ != nullptr)
      this->rpm_->publish_state((data[11] * 256) + data[12]);
    if (this->flow_ != nullptr)
      this->flow_->publish_state(data[13] * 0.227);  // GPM to m3/hr
    if (this->pressure_ != nullptr)
      this->pressure_->publish_state(data[14] / 14.504);  // PSI to bar
    //
    //
    // this->timer_hour = packet.data[11];
    // this->timer_min = packet.data[12];
    // this->hour = packet.data[13];
    // this->minutes = packet.data[14];

    // ESP_LOGI(TAG, "R: 0x%02x - M: 0x%02x (%s) - St: 0x%02x", pc->running, pc->mode, pc->getMode()._to_string(),
    //          pc->status);
    // ESP_LOGI(TAG, "P: %dW - Sp: %dtr/min - F: %.2fm3/h - P: %.2fbar", pc->watts, pc->rpm, pc->flow, pc->pressure);
    // ESP_LOGI(TAG, "B9: 0x%02x - B10: 0x%02x - Tmr: %d:%02d - Hr: %d:%02d", packet.data[9], packet.data[10],
    //          pc->timer_hour, pc->timer_min, pc->hour, pc->minutes);
  } else if (data[3] == 0x60 && data[4] == 0x09) {
    // ESP_LOGI(TAG, "Pump goes to flow mode: %0.1f m3/h", ((double) packet.data[1]) / 10);
  } else if (data[3] == 0x60 && data[4] == 0x0A) {
    // ESP_LOGI(TAG, "Pump goes to rpm mode: %d rpm", (packet.data[0] * 256) + packet.data[1]);
    // } else if (data[3] == 0x60 && packet.length == 1 && data[4] == 0xFF) {
    //   ESP_LOGW(TAG, "CMD error: %02x", packet.data[0]);
  } else {
    // if (Debug.isActive(Debug.INFO)) {
    //   char hd_buffer[(6 + packet.length + 2) * 3];
    //   char *ptr_hd;
    //   ptr_hd = hd_buffer;
    //   for (int j = start; j < start + 6 + packet.length + 2; j++) {
    //     ptr_hd += sprintf(ptr_hd, " %02x", (unsigned char) data[j]);
    // }
    // ESP_LOGI(TAG, "undecoded packet:%s", hd_buffer);
  }
}

void Intelliflo::send_array_cmd(std::vector<uint8_t> data) {
  return send_array_cmd((const uint8_t *) data.data(), data.size());
}

void Intelliflo::send_array_cmd(const uint8_t *data, size_t len) {
  this->flush();

  this->write_array(&data[0], len);

  // print to log
  std::string pretty_cmd = format_hex_pretty((uint8_t *) &data[0], len);
  ESP_LOGI(TAG, "Sent: %S ", pretty_cmd.c_str());
}

void Intelliflo::update() {
  this->requestPumpStatus();
  this->pumpToLocalControl();
}

// request pump status
void Intelliflo::requestPumpStatus() {
  ESP_LOGI(TAG, "Requesting pump status");
  uint8_t statusPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x07, 0x00};
  QueuePacket(statusPacket, 6);
}

// set pump to local control
void Intelliflo::pumpToLocalControl() {
  ESP_LOGI(TAG, "Requesting local control");
  uint8_t localControlPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x04, 0x01, 0x00};
  QueuePacket(localControlPacket, 7);
}

// set pump to remote control
void Intelliflo::pumpToRemoteControl() {
  ESP_LOGI(TAG, "Requesting remote control");
  uint8_t remoteControlPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x04, 0x01, 0xFF};
  QueuePacket(remoteControlPacket, 7);
}

// turn pump on/off
void Intelliflo::run() {
  ESP_LOGI(TAG, "Run Pump");
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x06, 0x01, 0x0A};
  QueuePacket(pumpPowerPacket, 7);
}

void Intelliflo::stop() {
  ESP_LOGI(TAG, "Stop Pump");
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x06, 0x01, 0x04};
  QueuePacket(pumpPowerPacket, 7);
}

void Intelliflo::commandLocalProgram(int prog) {
  ESP_LOGI(TAG, "Command local program %d", prog);
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x05, 0x01, 0};
  pumpPowerPacket[6] = prog + 1;
  QueuePacket(pumpPowerPacket, 7);
}

void Intelliflo::commandExternalProgram(int prog) {
  ESP_LOGI(TAG, "Command external program %d", prog);
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x01, 0x04, 0x03, 0x21, 0x00, 0x00};
  pumpPowerPacket[9] = prog * 8;
  QueuePacket(pumpPowerPacket, 10);
}

void Intelliflo::saveValueForProgram(int prog, int value) {
  ESP_LOGI(TAG, "saveValueForProgram %d: %d", prog, value);
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x01, 0x04, 0x03, 0, 0, 0};
  pumpPowerPacket[7] = 0x26 + prog;
  pumpPowerPacket[8] = floor(value / 256);
  pumpPowerPacket[9] = value % 256;
  QueuePacket(pumpPowerPacket, 10);
}

void Intelliflo::commandRPM(int rpm) {
  ESP_LOGI(TAG, "Command RPM: %d rpm", rpm);
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x0A, 0x04, 0x02, 0xC4, 0, 0};
  pumpPowerPacket[8] = floor(rpm / 256);
  pumpPowerPacket[9] = rpm % 256;
  QueuePacket(pumpPowerPacket, 10);
}

void Intelliflo::commandFlow(int flow)  // In m3/H * 10
{
  ESP_LOGI(TAG, "Command Flow: %.1f m3/h", ((double) flow) / 10);
  uint8_t pumpPowerPacket[] = {0xA5, 0x00, 0x60, 0x10, 0x09, 0x04, 0x02, 0xC4, 0x00, 0};
  pumpPowerPacket[9] = flow;
  QueuePacket(pumpPowerPacket, 10);
}

void Intelliflo::QueuePacket(uint8_t message[], int messageLength) {
  ESP_LOGV(TAG, "queuePacket: Adding checksum and validating packet to be written ");
  ESP_LOGV(TAG, "queuePacket: message length: %d", messageLength);

  int checksum = 0;
  for (int j = 0; j < messageLength; j++) {
    checksum += message[j];
  }

  std::vector<uint8_t> packet = {0xFF, 0x00, 0xFF};
  int packetSize = 0;
  int requestGet = 0;

  packet.insert(packet.end(), message, message + messageLength);
  packet.push_back(checksum >> 8);
  packet.push_back(checksum & 0xFF);

  packetSize = messageLength + 3 + 2;

  //-------Internally validate checksum
  int len = 0, packetchecksum = 0, databytes = 0;

  // example packet: 255,0,255,165,10,16,34,2,1,0,0,228
  len = packetSize;
  // checksum is calculated by 256*2nd to last bit + last bit
  packetchecksum = (packet[len - 2] * 256) + packet[len - 1];
  databytes = 0;
  // add up the data in the payload
  for (int i = 3; i < len - 2; i++) {
    databytes += packet[i];
  }

  bool validPacket = (packetchecksum == databytes);
  if (!validPacket) {
    ESP_LOGW(TAG, "Asking to queue malformed packet");
  } else {
    tx_buffer.push(packet);
  }
}

}  // namespace intelliflo
}  // namespace esphome
