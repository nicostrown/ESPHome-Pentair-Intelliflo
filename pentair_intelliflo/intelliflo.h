#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
// #include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
// #include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <queue>

namespace esphome {
namespace intelliflo {

enum running : uint8_t {
  STOPPED = 0x04,
  RUNNING = 0x0A,
};

enum program : uint8_t {
  NO_PROG = 0x00,
  LOCAL1 = 0x01,
  LOCAL2 = 0x02,
  LOCAL3 = 0x03,
  LOCAL4 = 0x04,
  EXT1 = 0x09,
  EXT2 = 0x0A,
  EXT3 = 0x0B,
  EXT4 = 0x0C,
  TIMEOUT = 0x0E,
  PRIMING = 0x11,
  QUICKCLEAN = 0x0D,
  UNKNOWN = 0xFF,
};

class Intelliflo : public uart::UARTDevice, public PollingComponent {
  void switch_command(const std::string &command);
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

 protected:
  uint32_t last_received_byte_millis = 0;

  void send_array_cmd(std::vector<uint8_t> data);
  void send_array_cmd(const uint8_t *data, size_t len);

  void parse_packet(const std::vector<uint8_t> &data);  // parsing the status package

  void handle_received_byte(uint8_t c);  // received byte handler
  bool validate_received_message();      // function of checking the received message

  void query_status();

  void publish_state_if_changed();

  std::vector<uint8_t> rx_buffer;
  std::queue<std::vector<uint8_t>> tx_buffer;
  bool ready_to_tx = true;

  void QueuePacket(uint8_t message[], int messageLength);

  void requestPumpStatus();
  void pumpToLocalControl();
  void pumpToRemoteControl();
  void run();
  void stop();
  void commandLocalProgram(int prog);
  void commandExternalProgram(int prog);
  void saveValueForProgram(int prog, int value);
  void commandRPM(int rpm);
  void commandFlow(int flow);  // In m3/H * 10

  sensor::Sensor *power_;
  sensor::Sensor *rpm_;
  sensor::Sensor *flow_;
  sensor::Sensor *pressure_;

  binary_sensor::BinarySensor *running_;

  text_sensor::TextSensor *program_;

 public:
  void set_power(sensor::Sensor *sensor) { power_ = sensor; }
  void set_rpm(sensor::Sensor *sensor) { rpm_ = sensor; }
  void set_flow(sensor::Sensor *sensor) { flow_ = sensor; }
  void set_pressure(sensor::Sensor *sensor) { pressure_ = sensor; }

  void set_running(binary_sensor::BinarySensor *sensor) { running_ = sensor; }

  void set_program(text_sensor::TextSensor *sensor) { program_ = sensor; }
};

}  // namespace intelliflo
}  // namespace esphome
