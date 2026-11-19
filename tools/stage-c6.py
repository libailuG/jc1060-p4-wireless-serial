"""Stage a C6 app over the P4 console with a bounded USB receive window."""
import argparse
import hashlib
from pathlib import Path
import re
import time
import serial

ap = argparse.ArgumentParser()
ap.add_argument('image', type=Path)
ap.add_argument('--port', default='COM13')
ap.add_argument('--log', type=Path, required=True)
ap.add_argument('--ota', action='store_true')
a = ap.parse_args()
data = a.image.read_bytes()
assert 256 <= len(data) <= 1800000 and data[0] == 0xe9 and data[12:14] == b'\x0d\x00'
log = bytearray()

def connect():
    p = serial.Serial(port=None, baudrate=115200, timeout=.2)
    p.dtr = False
    p.rts = False
    p.port = a.port
    p.open()
    return p

def wait_for(p, pattern, timeout=10):
    start = time.monotonic()
    received = bytearray()
    while time.monotonic() - start < timeout:
        chunk = p.read(p.in_waiting or 1)
        log.extend(chunk)
        received.extend(chunk)
        if re.search(pattern, received):
            return bytes(received)
    raise RuntimeError('Timeout: ' + repr(pattern) + '\n' + received[-1800:].decode('utf8', errors='replace'))

p = connect()
try:
    for attempt in range(15):
        p.write(f'\nc6 stage {len(data)} {hashlib.sha256(data).hexdigest()}\n'.encode())
        try:
            wait_for(p, rb'STAGE READY', 3)
            break
        except RuntimeError:
            if attempt == 14: raise
    for offset in range(0, len(data), 64):
        block = data[offset:offset+64]
        end = offset + len(block)
        for attempt in range(8):
            p.write(f'\nc6 data {offset} {block.hex()}\n'.encode())
            try:
                received = wait_for(p, f'STAGE ACK {end}\\b'.encode(), 2)
                break
            except RuntimeError:
                if attempt == 7: raise
        if offset == 0 or offset // 262144 != end // 262144:
            print(f'Staged {end}/{len(data)}', flush=True)
        if end == len(data) and b'STAGE VERIFIED' not in received:
            wait_for(p, rb'STAGE VERIFIED')
    print('Full image SHA256 verified', flush=True)
    if a.ota:
        p.write(b'c6 ota\n')
        result = wait_for(p, rb'OTA VERIFIED|C6 OTA: [^\r\n]+', 180)
        print(result[-1800:].decode('utf8', errors='replace'), flush=True)
        if b'OTA VERIFIED' not in result:
            raise RuntimeError('C6 OTA failed')
        p.close()
        # The new C6 image keeps the previous OTA slot and reverts if the host
        # does not prove its custom channel within 90 seconds.
        time.sleep(3)
        deadline = time.monotonic() + 70
        while time.monotonic() < deadline:
            try:
                p = connect()
                p.write(b'now start\n')
                result = wait_for(p, rb'ESP-NOW: ESP_OK|ESP-NOW: ESP_ERR_[A-Z_]+', 8)
                print(result[-1400:].decode('utf8', errors='replace'), flush=True)
                if b'ESP-NOW: ESP_OK' in result:
                    p.write(b'wifi mac\nnow ui\n')
                    print('C6 custom channel verified', flush=True)
                    break
            except (serial.SerialException, RuntimeError):
                pass
            finally:
                if p.is_open:
                    p.close()
            time.sleep(2)
        else:
            raise RuntimeError('New C6 custom channel not verified; recovery guard remains armed')
finally:
    if p.is_open:
        p.close()
    a.log.parent.mkdir(parents=True, exist_ok=True)
    a.log.write_bytes(log)
