# SensESP N2K Gateway

NMEA 2000 candump-over-TCP gateway for [SensESP](https://github.com/SignalK/SensESP). Turns an ESP32-P4 (or any ESP32 with TWAI) into an NMEA 2000 gateway that [canboatjs](https://github.com/canboat/canboatjs) can connect to.

## How It Works

```
NMEA 2000 Bus (250 kbps CAN)
       │
  [CAN transceiver]  (SN65HVD230 or TJA1051T)
       │
  ESP32-P4 (TWAI peripheral)
       │
  CandumpTcpServer (port 2599)
       │
  SignalK Server → canboatjs → tcp://esp32-ip:2599
```

The gateway streams raw CAN frames in candump ASCII format over TCP. SignalK's canboatjs plugin connects and decodes all PGNs — no parsing on the ESP32.

**Bidirectional:** also accepts inbound candump lines from clients and transmits them on the bus.

## Quick Start

Add to your `platformio.ini`:

```ini
lib_deps =
    SignalK/SensESP@>=3.3.0
    https://github.com/dirkwa/sensesp-n2k-gateway.git
```

```cpp
#include "sensesp_n2k_gateway.h"
#include "sensesp/net/ethernet_provisioner.h"
#include "sensesp_app_builder.h"

using namespace sensesp;

void setup() {
  auto app = SensESPAppBuilder()
      .set_hostname("n2k-gateway")
      .set_ethernet(EthernetConfig::waveshare_esp32p4_poe())
      .get_app();

  auto* receiver    = new TwaiReceiver({.tx_pin = GPIO_NUM_4, .rx_pin = GPIO_NUM_5});
  auto* transmitter = new TwaiTransmitter();
  auto* server      = new CandumpTcpServer(receiver, transmitter);

  receiver->start();
  transmitter->start();
  server->start();
}

void loop() { event_loop()->tick(); }
```

### SignalK Server Configuration

```json
{
  "id": "n2k-esp32-p4",
  "enabled": true,
  "type": "NMEA2000",
  "options": {
    "type": "ngt-1-canboatjs",
    "device": "tcp://n2k-gateway.local:2599"
  }
}
```

## Combined BLE + N2K Gateway

Both gateways run on a single ESP32-P4 board. See [examples/gateway_plus_ble/](examples/gateway_plus_ble/) — pull both libraries and instantiate both in `main.cpp`.

## Hardware

| Component | Notes |
|-----------|-------|
| ESP32-P4 board | Waveshare ESP32-P4-WIFI6-POE-ETH (or any with TWAI) |
| CAN transceiver | SN65HVD230 (3.3V) or TJA1051T/3 (3.3V logic, 5V bus) |
| TWAI pins | Any two GPIOs (default: TX=GPIO4, RX=GPIO5) |
| Bus termination | 120-ohm resistor if at end of backbone |
| Power | PoE or USB-C |

## Features

- **Candump ASCII protocol** — same format as Linux `candump` stdout
- **Bidirectional** — RX from bus + TX to bus via same TCP connection
- **Multi-client** — up to 8 concurrent TCP clients
- **Bus-off auto-recovery** — detects and recovers from CAN bus-off state
- **SensESP integration** — `TwaiReceiver` is a `ValueProducer<TwaiMessage>`, can pipe to SK delta producers

## Requirements

- SensESP >= 3.3.0
- PlatformIO with `framework = espidf, arduino` (pioarduino)
- signalk-server with canboatjs plugin
