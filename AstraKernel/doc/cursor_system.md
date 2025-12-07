# Mouse Cursor System with PNG Support

## Overview

The mouse cursor system supports loading PNG images directly from the filesystem using lodepng library. No kernel recompilation needed! The cursor is drawn with alpha blending support for transparency.

## Usage (Recommended: Filesystem-based)

### 1. Prepare PNG Image

Create a PNG image for your cursor (recommended: 16x16 to 32x32 pixels, RGBA format with transparency).

### 2. Place PNG in Filesystem

Place `cursor.png` in one of these locations (checked in order):
- `/assets/cursor.png` (recommended)
- `/usr/share/cursor.png`
- `/etc/cursor.png`
- `/cursor.png`

The kernel will automatically load it during initialization.

### 3. For Initrd/Embedded Systems

If you're using initrd, place `cursor.png` in the initrd as `/assets/cursor.png`:
```bash
mkdir -p initrd_root/assets
cp cursor.png initrd_root/assets/cursor.png
# Build initrd.img from initrd_root
```

## Alternative: Embedded Cursor (Optional)

If you want to embed the cursor in the kernel binary:

1. Convert PNG to C array:
```bash
xxd -i cursor.png > assets/cursor.h
```

2. Compile with `-DCURSOR_EMBEDDED` flag

3. The embedded cursor will be used as fallback if filesystem cursor is not found

## API Functions

- `mouse_cursor_load_from_file(path)` - Load cursor from filesystem path
- `mouse_cursor_load_from_memory(png_data, png_size)` - Load cursor from memory buffer
- `mouse_cursor_get_size(w, h)` - Get cursor dimensions
- `mouse_cursor_draw(x, y)` - Draw cursor at position
- `mouse_cursor_cleanup()` - Free cursor memory

## Fallback

If no cursor image is found, the mouse driver uses a simple 16x16 pixel fallback cursor.

## Technical Details

### Lodepng Integration

The system uses lodepng library for PNG decoding:
- Custom allocators (`lodepng_alloc.cpp`) redirect malloc/free to kernel `kmalloc`/`kfree`
- C++ linkage wrappers (`lodepng_wrapper.cpp`) provide C API for C code
- Disabled C++ stdlib dependencies and disk I/O

### Alpha Blending

The cursor drawing supports alpha blending:
- Fully transparent pixels (alpha=0) are skipped
- Partially transparent pixels are blended with background
- Fully opaque pixels are drawn directly

### Memory Management

- Cursor image data is allocated via `kmalloc`
- Background save/restore uses dynamic allocation based on cursor size
- Automatic cleanup on cursor reload or system shutdown

