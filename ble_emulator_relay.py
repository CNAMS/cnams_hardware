#!/usr/bin/env python3
"""
BLE to Android Emulator Relay Bridge for CNAMS Ankur.
Connects to physical ESP32-S3 via Host Bluetooth (bleak) and exposes a
TCP socket on 0.0.0.0:8765. The Android Emulator connects to 10.0.2.2:8765.

All incoming 11-byte BLE GATT frames are forwarded to the emulator in real time.
All control commands (0x02: Measure, 0x01: Zero) from the emulator are written
back to the ESP32 GATT control characteristic.
"""

import asyncio
import struct
from typing import Set
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
MEASURE_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Notify
CONTROL_CHAR_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8"  # Write

TCP_PORT = 8765

connected_clients: Set[asyncio.StreamWriter] = set()
ble_client: BleakClient = None
ble_lock = asyncio.Lock()


def on_ble_notification(sender, data: bytearray):
    """Callback when a notification arrives from ESP32."""
    if not connected_clients:
        return
    
    # Broadcast raw 11-byte frame to all connected emulator clients
    dead_clients = set()
    for writer in list(connected_clients):
        try:
            writer.write(data)
        except Exception:
            dead_clients.add(writer)
            
    for dead in dead_clients:
        connected_clients.discard(dead)


async def ensure_ble_connected():
    """Maintains connection to the ESP32-S3."""
    global ble_client
    async with ble_lock:
        if ble_client and ble_client.is_connected:
            return ble_client

        print("🔍 [BLE Relay] Scanning for ESP32 ('CGMS-ANKUR-S3')...")
        device = await BleakScanner.find_device_by_name("CGMS-ANKUR-S3", timeout=8.0)
        if not device:
            # Fallback scan by service UUID
            print("🔍 [BLE Relay] Fallback scanning by Service UUID...")
            devices = await BleakScanner.discover(timeout=5.0)
            for d in devices:
                if d.name and "CGMS" in d.name:
                    device = d
                    break

        if not device:
            print("⚠️ [BLE Relay] ESP32 not found. Is it powered on?")
            return None

        print(f"🔗 [BLE Relay] Connecting to {device.name} ({device.address})...")
        client = BleakClient(device)
        try:
            await client.connect()
            print("✅ [BLE Relay] Connected to ESP32 GATT Server!")
            await client.start_notify(MEASURE_CHAR_UUID, on_ble_notification)
            print(f"📡 [BLE Relay] Subscribed to Measurement Char ({MEASURE_CHAR_UUID})")
            ble_client = client
            return ble_client
        except Exception as e:
            print(f"❌ [BLE Relay] Connection error: {e}")
            ble_client = None
            return None


async def handle_emulator_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    """Handles an incoming TCP connection from the Android Emulator."""
    addr = writer.get_extra_info("peername")
    print(f"📱 [TCP Server] Emulator client connected from {addr}")
    connected_clients.add(writer)

    # Ensure BLE is connected when an emulator client connects
    await ensure_ble_connected()

    try:
        while True:
            data = await reader.read(1024)
            if not data:
                break
            
            # If emulator sent a control command (e.g. 0x01 or 0x02)
            if ble_client and ble_client.is_connected:
                print(f"⚡ [TCP Server -> BLE] Sending command to ESP32: {list(data)}")
                try:
                    await ble_client.write_gatt_char(CONTROL_CHAR_UUID, data, response=False)
                except Exception as ex:
                    print(f"⚠️ [BLE Write Error]: {ex}")
            else:
                print(f"⚠️ [TCP Server] Received command {list(data)} but BLE is not connected. Attempting reconnect...")
                client = await ensure_ble_connected()
                if client and client.is_connected:
                    await client.write_gatt_char(CONTROL_CHAR_UUID, data, response=False)

    except asyncio.CancelledError:
        pass
    except Exception as e:
        print(f"📱 [TCP Server] Client error: {e}")
    finally:
        print(f"📱 [TCP Server] Emulator client disconnected: {addr}")
        connected_clients.discard(writer)
        writer.close()
        await writer.wait_closed()


async def background_ble_watchdog():
    """Keeps BLE connection alive when emulator clients are connected."""
    while True:
        if connected_clients:
            if not ble_client or not ble_client.is_connected:
                await ensure_ble_connected()
        await asyncio.sleep(4.0)


async def main():
    print("==================================================")
    print("🚀 CNAMS BLE-to-Emulator TCP Relay Bridge")
    print(f"🌐 Listening on 0.0.0.0:{TCP_PORT} (Emulator: 10.0.2.2:{TCP_PORT})")
    print("==================================================")

    server = await asyncio.start_server(handle_emulator_client, "0.0.0.0", TCP_PORT)
    asyncio.create_task(background_ble_watchdog())

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Relay server stopped.")
