# Ankur (अंकुर) CGMS — ESP32-S3 Firmware (`cnams_hardware`)

Production-grade firmware (v1.5.0) for the **ESP32-S3 N16R8** (16MB Flash, 8MB Octal PSRAM) providing automated, BLE-connected anthropometric measurements (Weight via Load Cell / HX711 and Height via Rotary Encoder) for the **Ankur** Anganwadi Child Growth Monitoring mobile application (`cnams_app`).

---

## 1. Hardware Pinout & Wiring

| Peripheral | ESP32-S3 Pin | Notes |
|---|---|---|
| **BOOT Button** | `GPIO 0` | Built-in button. **Short Press**: Trigger Measurement Cycle; **Long Press (>2s)**: Tare / Zero. |
| **Onboard RGB LED (WS2812B)** | `GPIO 38` | Status indicator LED. |
| **OLED Display (I2C SSD1306)** | `GPIO 17` (SDA), `GPIO 18` (SCL) | 0.96" 128×64 Monochrome OLED local real-time display. |
| **HX711 Load Cell ADC (Weight)**| `GPIO 4` (DT), `GPIO 5` (SCK) | 24-bit ADC for load cells (0–50 kg). |
| **Rotary Encoder (Height)** | `GPIO 6` (Phase A), `GPIO 7` (Phase B)| Quadrature encoder for caliper / length board extension. |

---

## 2. OLED Display UI (SSD1306 128×64)

The 0.96" monochrome I2C OLED display runs at 10 Hz and handles multiple distinct operational screens:

1. **Splash Screen (Boot - 1.5s)**:
   - Displays device branding (`ANKUR CGMS`), hardware variant (`ESP32-S3`), firmware version (`v1.5.0`), and BLE advertising name (`CGMS-ANKUR-S3`).
2. **Idle Screen (BLE Disconnected / Scanning)**:
   - Top status header: `CGMS-ANKUR-S3 [BLE:ADV]`.
   - Placeholders: `WT: --- kg` and `HT: --- cm`.
   - Animated bottom indicator: `BLE scanning...`.
3. **Live Measurement Screen (BLE Connected)**:
   - Top status header: `CGMS-ANKUR-S3 [BLE: OK]`.
   - Real-time large text values: `WT <val> kg` and `HT <val> cm`.
   - Independent per-channel stability icons: animated tildes `~~~` cycle while readings fluctuate.
   - Bottom status line: `Measuring...` animation during active acquisition or `Hold BOOT=TARE` prompt.
4. **Locked / Stable Screen**:
   - Each channel that achieves stability displays an inverted `LOCK` badge.
   - Full-width inverted badge at bottom: `*** STABLE / LOCKED ***`.
5. **Tare / Zero Flash**:
   - Displays high-contrast notification `TARE / ZERO — Sensors Baseline Set` when long-press tare is triggered.

---

## 3. Interactive Features & Modes

### 3.1 Smart Simulation & Auto-Capture Mode
If physical sensors are not attached, the firmware automatically operates in **Demo/Auto-Capture Mode**:
1. When you press the **BOOT Button (GPIO 0)**, the firmware simulates an active measurement cycle:
   - Emits realistic jittering readings (`stable = false`) over ~1.5 seconds.
   - Locks onto a stable target value (`stable = true`).
   - Streams 11-byte BLE frames directly to `cnams_app`.
2. The **I2C OLED Screen** displays animated measurement dots and locks the final weight & height.
3. The **WS2812B RGB LED** changes color dynamically:
   - 🔵 **Pulsing Blue**: BLE Advertising / Waiting for connection.
   - 🟢 **Solid Green**: Connected to mobile app.
   - 🟡 **Blinking Yellow**: Measuring / Jittering in progress.
   - ⚪/🔵 **Cyan Flash**: Measurement locked and transmitted.
   - 🔴 **Red Flash**: Tare / Zero command executed.

### 3.2 Live Sensor Mode
When an **HX711** and **Rotary Encoder** are connected, the firmware automatically reads physical signals, applies moving-average filtering, calculates stability variance, and sends live data.

---

## 4. BLE GATT Specification

- **Device Name**: `CGMS-ANKUR-S3` (Matches `CGMS` prefix in `cnams_app`)
- **Service UUID**: `4FAFC201-1FB5-459E-8FCC-C5C9C331914B`
- **Measurement Characteristic (Notify)**: `BEB5483E-36E1-4688-B7F5-EA07361B26A8`
- **Control / Tare Characteristic (Write/Read)**: `BEB5483F-36E1-4688-B7F5-EA07361B26A8`

### 11-Byte Frame Layout (Big-Endian):
```
[0]     0xA5 (Start Marker)
[1]     Channel (0x00 = Weight in grams, 0x01 = Length in mm)
[2]     Flags (bit 0: 0x01 = Stable, 0x00 = Unstable)
[3..6]  int32 Value (Grams or Millimetres in Big-Endian)
[7..8]  uint16 Sequence Counter
[9..10] uint16 CRC-16/CCITT-FALSE over bytes [0..8] (Poly 0x1021, Init 0xFFFF)
```

---

## 5. How to Build & Flash

### Prerequisites
- Install [PlatformIO Core](https://platformio.org/) or the PlatformIO extension in VS Code.

### Commands:
```bash
cd cnams_hardware

# 1. Compile the firmware
pio run

# 2. Flash to connected ESP32-S3 over USB
pio run --target upload

# 3. Open Serial Monitor (115200 baud)
pio device monitor
```

---

## 6. Pairing with Mobile App (`cnams_app`)

1. Power on the ESP32-S3. The RGB LED will pulse **Blue**.
2. Open `cnams_app` on your Android/iOS phone.
3. In `cnams_app/lib/core/providers.dart`, set `deviceClientProvider` to use `RealDeviceClient` with the matching service and characteristic UUIDs.
4. Navigate to the Child Measurement screen — the app will discover `CGMS-ANKUR-S3`, connect (RGB turns **Green**), and auto-populate the weight and length fields when measurements are locked!
