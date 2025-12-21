import asyncio
import time
import sys
from bleak import BleakScanner, BleakClient

SERVICE_UUID = "0000b3a0-0000-1000-8000-00805f9b34fb"
NOTIFY_UUID  = "0000b3a1-0000-1000-8000-00805f9b34fb"
WRITE_UUID   = "0000b3a2-0000-1000-8000-00805f9b34fb"

packet_count = {}
start_time = None
spinner = ["|", "/", "-", "\\"]

def make_notify_handler(key):
    def handler(_, data):
        packet_count[key] += 1
    return handler

async def send(client, cmd):
    await client.write_gatt_char(WRITE_UUID, bytes(cmd), response=True)
    await asyncio.sleep(0.2)

async def run_device(device):
    key = device.address
    packet_count[key] = 0

    async with BleakClient(device.address) as client:
        print(f"Connected → {device.name} [{device.address}]")

        await client.start_notify(NOTIFY_UUID, make_notify_handler(key))

        # === CPP ENABLE COMMANDS ===
        await send(client, [0x55, 0xAA, 0xF0, 0x00])
        await send(client, [0x55, 0xAA, 0x11, 0x02, 0x00, 0x02])
        await send(client, [0x55, 0xAA, 0x0A, 0x00])
        await send(client, [0x55, 0xAA, 0x08, 0x00])
        await send(client, [0x55, 0xAA, 0x06, 0x00])

        while time.time() - start_time < 60:
            await asyncio.sleep(0.1)

        await client.stop_notify(NOTIFY_UUID)

async def progress_loop():
    i = 0
    while time.time() - start_time < 60:
        line = spinner[i % len(spinner)]
        stats = " | ".join(
            f"{k[-5:]}={v}" for k, v in packet_count.items()
        )
        sys.stdout.write(
            f"\rCollecting data... {line}  {stats}"
        )
        sys.stdout.flush()
        i += 1
        await asyncio.sleep(1)
    print()  # newline

async def main():
    global start_time

    count = int(input("Kitne devices chahiye? : ").strip())

    print("Scanning devices...")
    devices = await BleakScanner.discover(timeout=20)

    targets = [d for d in devices if d.name and "GMSync" in d.name][:count]

    if len(targets) < count:
        print(f"❌ Sirf {len(targets)} devices milay")
        return

    start_time = time.time()

    await asyncio.gather(
        progress_loop(),
        *(run_device(d) for d in targets)
    )

    print("\n====== FINAL PACKET COUNTS ======")
    for dev, cnt in packet_count.items():
        print(f"{dev} → packets = {cnt}")

asyncio.run(main())
