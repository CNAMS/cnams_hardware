import asyncio
import struct
from bleak import BleakClient, BleakScanner

# ── Ankur CGMS GATT Specifications ───────────────────────────────────────────
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
MEASURE_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Notify
CONTROL_CHAR_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8"  # Write/Read

def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). Matches Dart PacketCodec."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte & 0xFF) << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def decode_packet(sender, data: bytearray):
    if len(data) != 11:
        print(f"[ERR] Invalid length: {len(data)}")
        return

    marker = data[0]
    channel = data[1]
    flags = data[2]
    val_raw = struct.unpack(">i", data[3:7])[0]
    seq = struct.unpack(">H", data[7:9])[0]
    received_crc = struct.unpack(">H", data[9:11])[0]
    expected_crc = crc16_ccitt(data[0:9])

    crc_status = "PASS" if received_crc == expected_crc else "FAIL"
    channel_name = "WEIGHT (g)" if channel == 0 else "LENGTH (mm)"
    stable_str = "STABLE" if (flags & 0x01) else "JITTERING"
    
    if channel == 0:
        formatted = f"{val_raw / 1000.0:.2f} kg ({val_raw} g)"
    else:
        formatted = f"{val_raw / 10.0:.1f} cm ({val_raw} mm)"

    hex_dump = " ".join(f"0x{b:02X}" for b in data)
    print(f"[BLE Frame #{seq:04d}] {channel_name:12s}: {formatted:18s} | State: {stable_str:9s} | CRC: {crc_status} (0x{received_crc:04X}) | Hex: [{hex_dump}]")

async def main():
    print("==================================================")
    print("  ANKUR CGMS — Live BLE End-to-End Hardware Test")
    print("==================================================")
    print("[1/5] Scanning for 'CGMS-ANKUR-S3'...")

    device = await BleakScanner.find_device_by_name("CGMS-ANKUR-S3", timeout=8.0)
    if not device:
        print("[FAIL] Device 'CGMS-ANKUR-S3' not found! Make sure ESP32-S3 is powered on.")
        return

    print(f"[2/5] Found device: {device.name} [{device.address}]")
    print(f"[3/5] Connecting to GATT Server...")

    async with BleakClient(device) as client:
        print(f"      Connected: {client.is_connected}")
        print(f"[4/5] Subscribing to Measurement Characteristic ({MEASURE_CHAR_UUID})...")
        await client.start_notify(MEASURE_CHAR_UUID, decode_packet)

        print("\n--- Listening to initial readings (2 seconds) ---")
        await asyncio.sleep(2)

        print("\n[5/5] Sending TRIGGER CAPTURE CYCLE command (0x02) to Control Characteristic...")
        await client.write_gatt_char(CONTROL_CHAR_UUID, bytearray([0x02]))
        
        print("--- Receiving Measurement Cycle Stream (Jitter -> Lock) ---")
        await asyncio.sleep(3.5)

        print("\n--- Sending TARE & ZERO command (0x01)...")
        await client.write_gatt_char(CONTROL_CHAR_UUID, bytearray([0x01]))
        await asyncio.sleep(1.5)

        await client.stop_notify(MEASURE_CHAR_UUID)
        print("\n>>> LIVE TEST FINISHED SUCCESSFULLY! <<<")

if __name__ == "__main__":
    asyncio.run(main())
