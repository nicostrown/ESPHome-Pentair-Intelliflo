#pragma once

// #include "esphome/components/binary_sensor/binary_sensor.h"
// #include "esphome/components/sensor/sensor.h"
// #include "esphome/components/switch/switch.h"
// #include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
// #include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <queue>

namespace esphome {
namespace intelliflo {

class Intelliflo : public uart::UARTDevice, public PollingComponent {
  // PIPSOLAR_SENSOR(grid_rating_voltage, QPIRI, float)
  // PIPSOLAR_VALUED_TEXT_SENSOR(device_mode, QMOD, char)
  // PIPSOLAR_BINARY_SENSOR(power_saving, QFLAG, int)
  // PIPSOLAR_TEXT_SENSOR(last_qpigs, QPIGS)
  // PIPSOLAR_SWITCH(output_source_priority_utility_switch, QPIRI)

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

  void QueuePacket(byte message[], int messageLength);

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
};

}  // namespace intelliflo
}  // namespace esphome
