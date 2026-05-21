# SensESP N2K Gateway

IP-based NMEA 2000 gateway for [SensESP](https://github.com/SignalK/SensESP). Turns an ESP32-P4 (or any ESP32 with TWAI) into an NMEA 2000 gateway that [canboatjs](https://github.com/canboat/canboatjs) can connect to as an `N2kIpGateway` source.

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
  SignalK Server → canboatjs N2kIpGateway → tcp://esp32-ip:2599
```

The gateway streams raw CAN frames in the [`candump3`](https://github.com/canboat/canboatjs/blob/master/lib/stringMsg.ts) canboat string format (one frame per line: `(seconds.microseconds) iface CANID#HEXDATA`) over TCP. The SignalK server's `N2kIpGateway` source (added in [canboatjs PR #437](https://github.com/canboat/canboatjs/pull/437), wired into the admin UI by [signalk-server PR #2694](https://github.com/SignalK/signalk-server/pull/2694)) connects in, parses each line, and decodes all PGNs — no parsing on the ESP32.

**Bidirectional:** also accepts inbound `candump3` lines from clients and transmits them on the bus.

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

In the SignalK admin UI under **Server → Connections → Add**, pick **NMEA 2000** as the input type, then choose **N2K IP Gateway (canboatjs)** from the source dropdown.

| Field | Value |
|-------|-------|
| Host | `n2k-gateway.local` (or the device's IP) |
| Port | `2599` |
| Text format | `candump3` (default) |

The equivalent `settings.json` entry:

```json
{
  "id": "n2k-esp32-p4",
  "enabled": true,
  "pipedProviders": [
    {
      "type": "providers/simple",
      "options": {
        "type": "NMEA2000",
        "subOptions": {
          "type": "n2k-ip-gateway-canboatjs",
          "host": "n2k-gateway.local",
          "port": 2599,
          "format": "candump3"
        }
      }
    }
  ]
}
```

Requires SignalK server with [PR #2694](https://github.com/SignalK/signalk-server/pull/2694) merged (until then, build from the `feat-n2k-ip-gateway` branch) and `@canboat/canboatjs` with `N2kIpGateway` exported (post [PR #437](https://github.com/canboat/canboatjs/pull/437)).

## Combined BLE + N2K Gateway

Both gateways run on a single ESP32-P4 board. See [examples/gateway_plus_ble/](examples/gateway_plus_ble/) — pull both libraries and instantiate both in `main.cpp`.

## Hardware

### Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (recommended)

CAN bus ready out of the box — built-in TJA1051T transceiver with PH2.0 connector for CANH/CANL. Default pin config (`GPIO22` TX, `GPIO21` RX) matches this board.

```cpp
auto* receiver = new TwaiReceiver(TwaiReceiverConfig::waveshare_touch_lcd_7b());
```

### Waveshare ESP32-P4-WIFI6-POE-ETH (with external transceiver)

No CAN transceiver on board. Add an SN65HVD230 breakout module connected to any two GPIOs on the 40-pin header.

```cpp
auto* receiver = new TwaiReceiver({.tx_pin = GPIO_NUM_4, .rx_pin = GPIO_NUM_5});
```

### General requirements

| Component | Notes |
|-----------|-------|
| Bus termination | 120-ohm resistor if at end of NMEA 2000 backbone |
| Power | PoE, USB-C, or NMEA 2000 bus power (with regulator) |

## Features

- **`candump3` canboat string format** — `(sec.usec) iface CANID#HEXDATA`, one frame per line, identical to `candump -L` log output
- **Bidirectional** — RX from bus + TX to bus via same TCP connection
- **Multi-client** — up to 8 concurrent TCP clients
- **Bus-off auto-recovery** — detects and recovers from CAN bus-off state
- **SensESP integration** — `TwaiReceiver` is a `ValueProducer<TwaiMessage>`, can pipe to SK delta producers

## Requirements

- SensESP >= 3.3.0
- PlatformIO with `framework = espidf, arduino` (pioarduino)
- signalk-server with the `N2kIpGateway` source (PR #2694 / canboatjs PR #437)
