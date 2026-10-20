# PMU Firmware

ESP32 firmware for simulating power-system measurements and transmitting them over **Ethernet (RMII)** using **UDP**. Built with **ESP-IDF**, C++, and FreeRTOS.

## Features

- Simulates five power-system devices, each running in a separate FreeRTOS task:
  - **PMU:** Three-phase voltage and current phasors (magnitudes and angles)
  - **Battery:** Voltage, current, and temperature
  - **Solar PV:** Voltage and current
  - **Wind turbine:** Voltage, current, power, wind speed, rotor RPM, and ambient temperature
  - **Three-phase AC:** RMS voltage/current, active/reactive power, power factor, and frequency
- Sends timestamped binary UDP packets every **5 seconds per device**, with small random variations in the measurements.
- Uses a **LAN8720 Ethernet PHY** with a static IP configuration.
- Supports optional SNTP time synchronization; otherwise initializes the clock from firmware build time.

## Default Network Configuration

| Setting | Value |
| --- | --- |
| ESP32 IP | `192.168.10.2` |
| Gateway / UDP destination | `192.168.10.1` |
| Subnet mask | `255.255.255.0` |
| UDP destination port | `9010` |
| RMII MDC / MDIO / PHY reset | GPIO `23` / `18` / `16` |

Edit these settings in `main/main.cpp` to match your setup.

## Packet Format

Each UDP datagram contains one device's measurements in a compact binary format:

| Field | Size |
| --- | --- |
| Packet type | 1 byte |
| Device ID | 4 bytes |
| Timestamp (Unix epoch, double) | 8 bytes |
| Measurement payload (32-bit floats) | Variable |

Multi-byte values are serialized in big-endian byte order. Packet types are `1` (PMU), `2` (Battery), `3` (PV), `4` (AC), and `5` (Wind).

## Build and Flash

With [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) installed and configured:

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

Connect the ESP32 Ethernet interface and run a UDP receiver on the configured destination IP and port to collect the packets.

## Source Layout

```text
main/
├── main.cpp                   # Simulation tasks and UDP transmission
├── Data_packet/
│   ├── Inc/data_packet.h      # Packet types and measurement structures
│   └── Src/data_packet.cpp    # Binary serialization
└── Eth_rmii/
    ├── Inc/eth_rmii.h         # Ethernet interface
    └── Src/eth_rmii.cpp       # RMII Ethernet initialization and events
```

> **Note:** Measurements are simulated, not acquired from physical sensors. This firmware is intended for development and testing of power-monitoring data pipelines.
