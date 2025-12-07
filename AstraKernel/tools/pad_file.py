#!/usr/bin/env python3
"""
Pad a file to a specific size by appending null bytes.
Usage: pad_file.py <file> <target_size>
"""
import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <file> <target_size>", file=sys.stderr)
    sys.exit(1)

filename = sys.argv[1]
target_size = int(sys.argv[2])

try:
    with open(filename, 'r+b') as f:
        f.seek(0, 2)  # Seek to end
        current_size = f.tell()
        if current_size < target_size:
            padding_size = target_size - current_size
            f.write(b'\x00' * padding_size)
        elif current_size > target_size:
            f.truncate(target_size)
except Exception as e:
    print(f"Error padding file: {e}", file=sys.stderr)
    sys.exit(1)

