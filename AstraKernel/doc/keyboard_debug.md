# Keyboard Debugging Guide

## Problem: Wrong characters appearing on real hardware

If keyboard produces wrong characters on real hardware, follow these steps:

### 1. Enable Debug Logging

Uncomment debug lines in `ps2_keyboard.c`:
- Line ~128: `printf("keyboard: scancode 0x%02x\n", sc);`
- Line ~110: `printf("keyboard: scancode 0x%02x -> '%c' (shift=%d)\n", sc, c, shift);`

### 2. Check Scancodes

When you press keys, you'll see output like:
```
keyboard: scancode 0x1E
keyboard: scancode 0x1E -> 'a' (shift=0)
```

### 3. Verify Scancode Set

The keyboard should be using scancode set 1. Check initialization logs:
```
keyboard: scancode set 1 configured
```

If you see "scancode set config skipped", the keyboard may be using a different set.

### 4. Common Issues

#### Issue: Controller translation disabled
**Symptom**: Scancodes don't match expected values
**Fix**: Ensure bit 6 in controller config is cleared (translation ON):
```c
cfg &= ~(1 << 6);   /* leave translation on (set1) */
```

#### Issue: Wrong scancode set
**Symptom**: Keys produce completely wrong characters
**Fix**: Keyboard may be using set 2 or 3. Check what scancodes you receive and adjust keymap accordingly.

#### Issue: Shift state not working
**Symptom**: Shift doesn't change characters
**Fix**: Check if scancodes 0x2A (left shift) and 0x36 (right shift) are being detected.

### 5. Scancode Set 1 Reference

Common scancodes (make codes):
- 0x01: Escape
- 0x02-0x0B: 1-9, 0
- 0x0C: Minus (-)
- 0x0D: Equals (=)
- 0x0E: Backspace
- 0x0F: Tab
- 0x10-0x19: Q-P
- 0x1A-0x1B: [, ]
- 0x1C: Enter
- 0x1E-0x26: A-L
- 0x27-0x29: ;, ', `
- 0x2B: Backslash (\)
- 0x2C-0x35: Z-M
- 0x36-0x37: ,, ., /
- 0x39: Space
- 0x2A, 0x36: Left/Right Shift

Break codes = make code | 0x80 (bit 7 set)

### 6. Testing

1. Press each key and note the scancode
2. Compare with expected scancode set 1 values
3. If scancodes don't match, keyboard may be using different set
4. Adjust keymap array indices to match received scancodes

