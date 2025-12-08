#pragma once

#include "types.h"

/**
 * USB Bit Manipulation Utilities
 * 
 * Provides macros and functions for bit manipulation in USB registers.
 */

/* Bit manipulation macros */
#define USB_BIT(n)           (1UL << (n))
#define USB_BITS(high, low)  (((1UL << ((high) - (low) + 1)) - 1) << (low))
#define USB_BIT_SET(val, bit)    ((val) |= USB_BIT(bit))
#define USB_BIT_CLEAR(val, bit)  ((val) &= ~USB_BIT(bit))
#define USB_BIT_TOGGLE(val, bit) ((val) ^= USB_BIT(bit))
#define USB_BIT_TEST(val, bit)   (((val) & USB_BIT(bit)) != 0)

/* Extract bits from value */
#define USB_BIT_EXTRACT(val, high, low) \
    (((val) >> (low)) & ((1UL << ((high) - (low) + 1)) - 1))

/* Insert bits into value */
#define USB_BIT_INSERT(val, bits, high, low) \
    ((val) = ((val) & ~USB_BITS(high, low)) | (((bits) << (low)) & USB_BITS(high, low)))

/* Read/write with bit manipulation */
#define USB_MMIO_READ_BITS(addr, high, low) \
    USB_BIT_EXTRACT(*(volatile uint32_t *)(addr), high, low)

#define USB_MMIO_WRITE_BITS(addr, bits, high, low) \
    do { \
        volatile uint32_t *reg = (volatile uint32_t *)(addr); \
        uint32_t val = *reg; \
        USB_BIT_INSERT(val, bits, high, low); \
        *reg = val; \
    } while (0)

/* Common USB bit positions */
#define USB_BIT_0   0
#define USB_BIT_1   1
#define USB_BIT_2   2
#define USB_BIT_3   3
#define USB_BIT_4   4
#define USB_BIT_5   5
#define USB_BIT_6   6
#define USB_BIT_7   7
#define USB_BIT_8   8
#define USB_BIT_9   9
#define USB_BIT_10  10
#define USB_BIT_11  11
#define USB_BIT_12  12
#define USB_BIT_13  13
#define USB_BIT_14  14
#define USB_BIT_15  15
#define USB_BIT_16  16
#define USB_BIT_31  31

/* USB register field definitions */
#define USB_REG_FIELD(reg, high, low) \
    USB_BIT_EXTRACT(reg, high, low)

#define USB_REG_FIELD_SET(reg, val, high, low) \
    USB_BIT_INSERT(reg, val, high, low)

