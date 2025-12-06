extern(C) void* k_memset(void* dest, int value, ulong n) @nogc nothrow @trusted
{
    auto d = cast(ubyte*)dest;
    auto val = cast(ubyte)value;

    // Align to 8 bytes
    while (n > 0 && (cast(size_t)d & 7) != 0) {
        *d++ = val;
        --n;
    }

    ulong pattern = val;
    pattern |= pattern << 8;
    pattern |= pattern << 16;
    pattern |= pattern << 32;

    auto d64 = cast(ulong*)d;
    while (n >= 8) {
        *d64++ = pattern;
        n -= 8;
    }

    d = cast(ubyte*)d64;
    while (n--) {
        *d++ = val;
    }
    return dest;
}
