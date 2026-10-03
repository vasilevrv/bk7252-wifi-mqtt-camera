#!/usr/bin/env python3
"""Pack BK7252 application blocks: 32 data bytes followed by big-endian CRC16."""
import argparse
from pathlib import Path
import struct

def crc16(block):
    crc = 0xffff
    for value in block:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x8005 if crc & 0x8000 else 0)) & 0xffff
    return crc

def pack(source, destination):
    if source.resolve() == destination.resolve():
        raise ValueError('Input and output must differ')
    data = source.read_bytes()
    if not data or len(data) > (0x1fe000 - 0x11000) * 32 // 34:
        raise ValueError('Application is empty or overlaps the settings region')
    with destination.open('wb') as out:
        for offset in range(0, len(data), 32):
            block = data[offset:offset+32].ljust(32, b'\xff')
            out.write(block + struct.pack('>H', crc16(block)))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    pack(args.source, args.destination)
    print('Packed application:', args.destination)
