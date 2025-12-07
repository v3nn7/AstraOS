#!/usr/bin/env python3
"""
Generate initrd archive (CPIO format) with assets for AstraOS kernel.
Based on OSDev Wiki CPIO format specification.
"""

import os
import sys
import struct
import argparse

def write_cpio_header(name, mode, uid, gid, size, mtime):
    """Write CPIO header in newc format"""
    # CPIO newc format header (110 bytes total)
    # Format: magic(6) + ino(8) + mode(8) + uid(8) + gid(8) + nlink(8) + mtime(8) + filesize(8) + devmajor(8) + devminor(8) + rdevmajor(8) + rdevminor(8) + namesize(8) + check(8)
    
    name_len = len(name) + 1  # +1 for null terminator
    name_padded = name.encode() + b"\0"
    # Pad to 4-byte boundary
    name_padded += b"\0" * ((4 - (len(name_padded) % 4)) % 4)
    
    header_len = 110 + len(name_padded)
    
    # Build header
    header_parts = [
        b"070701",  # magic (6 bytes)
        format(0, "08x").encode(),  # ino (8 bytes)
        format(mode, "08x").encode(),  # mode (8 bytes)
        format(uid, "08x").encode(),  # uid (8 bytes)
        format(gid, "08x").encode(),  # gid (8 bytes)
        format(0, "08x").encode(),  # nlink (8 bytes)
        format(mtime, "08x").encode(),  # mtime (8 bytes)
        format(size, "08x").encode(),  # filesize (8 bytes)
        format(0, "08x").encode(),  # devmajor (8 bytes)
        format(0, "08x").encode(),  # devminor (8 bytes)
        format(0, "08x").encode(),  # rdevmajor (8 bytes)
        format(0, "08x").encode(),  # rdevminor (8 bytes)
        format(name_len, "08x").encode(),  # namesize (8 bytes)
        format(0, "08x").encode(),  # check (8 bytes)
    ]
    
    header = b"".join(header_parts)
    
    return header + name_padded

def add_file_to_cpio(cpio_data, filepath, archive_path):
    """Add a file to CPIO archive"""
    if not os.path.exists(filepath):
        print(f"Warning: {filepath} does not exist, skipping")
        return
    
    stat = os.stat(filepath)
    size = stat.st_size
    mode = 0o100644  # Regular file, rw-r--r--
    uid = 0
    gid = 0
    mtime = int(stat.st_mtime)
    
    # Write header
    header = write_cpio_header(archive_path, mode, uid, gid, size, mtime)
    cpio_data.write(header)
    
    # Write file data
    with open(filepath, "rb") as f:
        data = f.read()
        cpio_data.write(data)
        # Pad to 4-byte boundary
        padding = (4 - (len(data) % 4)) % 4
        cpio_data.write(b"\0" * padding)

def add_directory_to_cpio(cpio_data, archive_path):
    """Add a directory entry to CPIO archive"""
    mode = 0o40755  # Directory, rwxr-xr-x
    uid = 0
    gid = 0
    mtime = 0  # Use current time or 0
    
    header = write_cpio_header(archive_path, mode, uid, gid, 0, mtime)
    cpio_data.write(header)

def main():
    parser = argparse.ArgumentParser(description="Generate initrd CPIO archive")
    parser.add_argument("--output", "-o", required=True, help="Output initrd file")
    parser.add_argument("--assets-dir", default="assets", help="Assets directory")
    args = parser.parse_args()
    
    output_path = args.output
    assets_dir = args.assets_dir
    
    # Create output directory if needed
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    
    print(f"Generating initrd: {output_path}")
    print(f"Assets directory: {assets_dir}")
    
    with open(output_path, "wb") as cpio:
        # Add root directory
        add_directory_to_cpio(cpio, "/")
        
        # Add /assets directory
        add_directory_to_cpio(cpio, "/assets")
        
        # Add cursor.png if it exists
        cursor_png = os.path.join(assets_dir, "cursor.png")
        if os.path.exists(cursor_png):
            print(f"Adding {cursor_png} -> /assets/cursor.png")
            add_file_to_cpio(cpio, cursor_png, "/assets/cursor.png")
        else:
            print(f"Warning: {cursor_png} not found, skipping")
        
        # Add TRAILER!!! (end of archive marker)
        trailer = write_cpio_header("TRAILER!!!", 0, 0, 0, 0, 0)
        cpio.write(trailer)
    
    # Get file size
    size = os.path.getsize(output_path)
    print(f"Initrd created: {output_path} ({size} bytes)")

if __name__ == "__main__":
    main()
