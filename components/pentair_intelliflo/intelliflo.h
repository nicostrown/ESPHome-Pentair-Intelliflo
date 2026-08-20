#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace pentair_intelliflo {

// Byte [0] of the 0x07 status payload.
static const uint8_t PUMP_STOPPED = 0x04;
static const uint8_t PUMP_STARTED = 0x0A;

class PentairIntelliflo : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_address(uint8_t address) { this->address_ = address; }

  void set_power_sensor(sensor::Sensor *s) { this->power_ = s; }
  void set_rpm_sensor(sensor::Sensor *s) { this->rpm_ = s; }
  void set_flow_sensor(sensor::Sensor *s) { this->flow_ = s; }
  void set_filter_percent_sensor(sensor::Sensor *s) { this->filter_percent_ = s; }
  void set_error_code_sensor(sensor::Sensor *s) { this->error_code_ = s; }
  void set_time_remaining_sensor(sensor::Sensor *s) { this->time_remaining_ = s; }

  void set_running_binary_sensor(binary_sensor::BinarySensor *s) { this->running_ = s; }
  void set_remote_control_binary_sensor(binary_sensor::BinarySensor *s) { this->remote_control_ = s; }

  void set_program_text_sensor(text_sensor::TextSensor *s) { this->program_ = s; }
  void set_pump_state_text_sensor(text_sensor::TextSensor *s) { this->pump_state_ = s; }
  void set_error_text_sensor(text_sensor::TextSensor *s) { this->error_ = s; }

  // --- Commands. All of these queue a frame; they never block. ---

  /// Ask the pump for a 0x07 status frame.
  void request_status();
  /// true locks the pump's own keypad and hands control to us (0x04 / 0xFF).
  void set_remote_control(bool remote);
  /// Start (0x0A) or stop (0x04) the drive (0x06).
  void set_pump_running(bool running);
  /// Volatile "run at this speed" (0x01 / 0x02C4). Does not touch pump EEPROM.
  void set_speed_rpm(uint16_t rpm);
  /// Volatile "run at this flow" in GPM (0x01 / 0x02E4). VF/VSF pumps only.
  void set_speed_gpm(uint8_t gpm);
  /// Store a speed in External Program 1-4 (0x01 / 0x0327-0x032A). Writes EEPROM.
  void set_program_speed(uint8_t program, uint16_t rpm);
  /// Activate External Program 1-4, or 0 to deactivate (0x01 / 0x0321).
  /// The pump drops an externally activated program after ~1 minute, so this
  /// has to be repeated while the program should stay active.
  void run_program(uint8_t program);
  /// Select one of the pump's built-in speeds (0x05).
  void set_speed_index(uint8_t index);

  // --- Aliases matching the upstream component's method names. ---
  void requestPumpStatus() { this->request_status(); }                       // NOLINT
  void pumpToRemoteControl() { this->set_remote_control(true); }             // NOLINT
  void pumpToLocalControl() { this->set_remote_control(false); }             // NOLINT
  void run() { this->set_pump_running(true); }
  void stop() { this->set_pump_running(false); }
  void commandRPM(int rpm) { this->set_speed_rpm(rpm < 0 ? 0 : (uint16_t) rpm); }   // NOLINT
  void commandFlow(int gpm) { this->set_speed_gpm(gpm < 0 ? 0 : (uint8_t) gpm); }   // NOLINT
  void commandExternalProgram(int prog) { this->run_program((uint8_t) prog); }      // NOLINT
  void saveValueForProgram(int prog, int value) {                                   // NOLINT
    this->set_program_speed((uint8_t) prog, value < 0 ? 0 : (uint16_t) value);
  }

  // --- Last known state, for use from lambdas. ---
  bool is_running() const { return this->running_state_; }
  bool is_remote_control() const { return this->remote_state_; }
  uint16_t current_rpm() const { return this->rpm_state_; }
  uint16_t current_watts() const { return this->watts_state_; }

 protected:
  void queue_command_(uint8_t command, const std::vector<uint8_t> &payload);
  void feed_byte_(uint8_t byte);
  bool resync_();
  void handle_frame_(size_t total);
  void publish_status_(const uint8_t *payload);

  uint8_t address_{0x60};

  std::vector<uint8_t> rx_;
  std::deque<std::vector<uint8_t>> tx_queue_;
  uint32_t last_rx_ms_{0};
  uint32_t last_tx_ms_{0};

  bool running_state_{false};
  bool remote_state_{false};
  uint16_t rpm_state_{0};
  uint16_t watts_state_{0};

  sensor::Sensor *power_{nullptr};
  sensor::Sensor *rpm_{nullptr};
  sensor::Sensor *flow_{nullptr};
  sensor::Sensor *filter_percent_{nullptr};
  sensor::Sensor *error_code_{nullptr};
  sensor::Sensor *time_remaining_{nullptr};
  binary_sensor::BinarySensor *running_{nullptr};
  binary_sensor::BinarySensor *remote_control_{nullptr};
  text_sensor::TextSensor *program_{nullptr};
  text_sensor::TextSensor *pump_state_{nullptr};
  text_sensor::TextSensor *error_{nullptr};
};

}  // namespace pentair_intelliflo
}  // namespace esphome
