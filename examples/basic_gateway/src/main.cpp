/**
 * @file main.cpp
 * @brief NMEA 2000 candump-over-TCP gateway on ESP32-P4.
 *
 * Streams raw CAN frames from the NMEA 2000 bus to TCP clients
 * in candump ASCII format. SignalK's canboatjs connects to
 * tcp://hostname:2599 and treats it as a CAN source.
 *
 * Also accepts inbound candump lines from clients and transmits
 * them on the bus (bidirectional).
 *
 * Hardware:
 *   - Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (built-in TJA1051T CAN
 *     transceiver on GPIO22/21, PH2.0 connector for CANH/CANL)
 *   - Also works on ESP32-P4-WIFI6-POE-ETH with external transceiver
 *   - NMEA 2000 backbone connection with 120-ohm termination
 */

#include "sensesp_n2k_gateway.h"

#include "sensesp/net/ethernet_provisioner.h"
#include "sensesp_app_builder.h"

using namespace sensesp;

void setup() {
  SetupLogging(ESP_LOG_INFO);

  SensESPAppBuilder builder;
  auto app = builder.set_hostname("n2k-gateway")
                 ->set_ethernet(EthernetConfig::waveshare_esp32p4_poe())
                 ->disable_wifi()
                 ->enable_ota("n2k-gw-ota")
                 ->get_app();

  // Built-in CAN transceiver on the Touch-LCD-7B (GPIO22 TX, GPIO21 RX)
  auto* receiver = new TwaiReceiver(TwaiReceiverConfig::waveshare_touch_lcd_7b());
  auto* transmitter = new TwaiTransmitter();
  auto* server = new CandumpTcpServer(receiver, transmitter);

  receiver->start();
  transmitter->start();
  server->start();

  event_loop()->onRepeat(5000, [receiver, transmitter, server]() {
    ESP_LOGI("N2K",
             "alive — rx=%u tx=%u tx_fail=%u bus_off=%u clients=%u",
             (unsigned)receiver->rx_count(),
             (unsigned)transmitter->tx_count(),
             (unsigned)transmitter->tx_fail_count(),
             (unsigned)receiver->bus_off_count(),
             (unsigned)server->connected_clients());
  });
}

void loop() { event_loop()->tick(); }
