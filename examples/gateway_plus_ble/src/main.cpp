/**
 * @file main.cpp
 * @brief Combined NMEA 2000 + BLE gateway on ESP32-P4.
 *
 * Runs both gateways on a single Waveshare ESP32-P4-WIFI6-POE-ETH:
 *   - NMEA 2000 candump-over-TCP on port 2599 (canboatjs)
 *   - BLE advertisements forwarded to signalk-server's BLE provider API
 *
 * Hardware:
 *   - Waveshare ESP32-P4-WIFI6-POE-ETH (Ethernet)
 *   - SN65HVD230 CAN transceiver on GPIO4 (TX) / GPIO5 (RX)
 *   - 2.4 GHz antenna on C6 IPEX connector (BLE)
 */

#include "sensesp_n2k_gateway.h"
#include "sensesp_ble_gateway/ble_signalk_gateway.h"
#include "sensesp_ble_gateway/esp_hosted_bluedroid_ble.h"

#include "sensesp/net/ethernet_provisioner.h"
#include "sensesp_app_builder.h"

using namespace sensesp;

static std::shared_ptr<EspHostedBluedroidBLE> g_ble;
static std::shared_ptr<BLESignalKGateway> g_ble_gateway;

void setup() {
  SetupLogging(ESP_LOG_INFO);

  SensESPAppBuilder builder;
  auto app = builder.set_hostname("p4-dual-gateway")
                 ->set_ethernet(EthernetConfig::waveshare_esp32p4_poe())
                 ->disable_wifi()
                 ->enable_ota("dual-gw-ota")
                 ->get_app();

  // --- NMEA 2000 gateway ---
  TwaiReceiverConfig twai_cfg;
  twai_cfg.tx_pin = GPIO_NUM_4;
  twai_cfg.rx_pin = GPIO_NUM_5;

  auto* receiver = new TwaiReceiver(twai_cfg);
  auto* transmitter = new TwaiTransmitter();
  auto* n2k_server = new CandumpTcpServer(receiver, transmitter);

  receiver->start();
  transmitter->start();
  n2k_server->start();

  // --- BLE gateway ---
  g_ble = std::make_shared<EspHostedBluedroidBLE>();
  g_ble_gateway =
      std::make_shared<BLESignalKGateway>(g_ble, app->get_ws_client());
  g_ble_gateway->start();

  // --- Heartbeat ---
  event_loop()->onRepeat(5000, [receiver, n2k_server]() {
    ESP_LOGI("GW",
             "n2k: rx=%u clients=%u | ble: hits=%u posted=%u ws=%d",
             (unsigned)receiver->rx_count(),
             (unsigned)n2k_server->connected_clients(),
             (unsigned)(g_ble ? g_ble->scan_hit_count() : 0),
             (unsigned)(g_ble_gateway ? g_ble_gateway->advertisements_posted()
                                     : 0),
             (int)(g_ble_gateway ? g_ble_gateway->control_ws_connected()
                                : false));
  });
}

void loop() { event_loop()->tick(); }
