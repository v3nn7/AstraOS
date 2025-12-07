/**
 * Input Subsystem Core Implementation
 * 
 * Unified input event layer for all input devices.
 */

#include "input/input_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

#define INPUT_EVENT_QUEUE_SIZE 256

/* Input Event Queue */
static input_event_t input_event_queue[INPUT_EVENT_QUEUE_SIZE];
static uint32_t input_event_head = 0;
static uint32_t input_event_tail = 0;
static bool input_event_queue_full = false;

/* Input Device List */
static input_device_t *input_devices = NULL;
static bool input_core_initialized = false;

/**
 * Initialize input subsystem
 */
int input_core_init(void) {
    if (input_core_initialized) {
        return 0;
    }
    
    k_memset(input_event_queue, 0, sizeof(input_event_queue));
    input_event_head = 0;
    input_event_tail = 0;
    input_event_queue_full = false;
    input_devices = NULL;
    input_core_initialized = true;
    
    klog_printf(KLOG_INFO, "input: core initialized");
    return 0;
}

/**
 * Cleanup input subsystem
 */
void input_core_cleanup(void) {
    input_device_t *dev = input_devices;
    while (dev) {
        input_device_t *next = dev->next;
        kfree(dev);
        dev = next;
    }
    
    input_devices = NULL;
    input_core_initialized = false;
    klog_printf(KLOG_INFO, "input: core cleaned up");
}

/**
 * Register input device
 */
input_device_t *input_device_register(input_device_type_t type, const char *name) {
    if (!input_core_initialized) {
        if (input_core_init() < 0) {
            return NULL;
        }
    }
    
    input_device_t *dev = (input_device_t *)kmalloc(sizeof(input_device_t));
    if (!dev) {
        return NULL;
    }
    
    k_memset(dev, 0, sizeof(input_device_t));
    dev->type = type;
    dev->name = name;
    dev->enabled = true;
    
    /* Add to device list */
    dev->next = input_devices;
    input_devices = dev;
    
    klog_printf(KLOG_INFO, "input: registered device '%s' (type=%u)", name, type);
    return dev;
}

/**
 * Unregister input device
 */
void input_device_unregister(input_device_t *dev) {
    if (!dev) return;
    
    /* Remove from device list */
    input_device_t **prev = &input_devices;
    while (*prev) {
        if (*prev == dev) {
            *prev = dev->next;
            break;
        }
        prev = &(*prev)->next;
    }
    
    kfree(dev);
    klog_printf(KLOG_INFO, "input: unregistered device");
}

/**
 * Submit input event
 */
void input_event_submit(input_device_t *dev, const input_event_t *event) {
    if (!dev || !event || !dev->enabled) return;
    
    /* Check if queue is full */
    if (input_event_queue_full) {
        klog_printf(KLOG_WARN, "input: event queue full, dropping event");
        return;
    }
    
    /* Add event to queue */
    uint32_t next_head = (input_event_head + 1) % INPUT_EVENT_QUEUE_SIZE;
    if (next_head == input_event_tail) {
        input_event_queue_full = true;
        return;
    }
    
    memcpy(&input_event_queue[input_event_head], event, sizeof(input_event_t));
    input_event_head = next_head;
}

/**
 * Poll input event
 */
bool input_event_poll(input_event_t *event) {
    if (!event) return false;
    
    if (input_event_tail == input_event_head && !input_event_queue_full) {
        return false; /* Queue empty */
    }
    
    /* Get event from queue */
    memcpy(event, &input_event_queue[input_event_tail], sizeof(input_event_t));
    
    input_event_tail = (input_event_tail + 1) % INPUT_EVENT_QUEUE_SIZE;
    input_event_queue_full = false;
    
    return true;
}

/**
 * Flush event queue
 */
void input_event_flush(void) {
    input_event_head = 0;
    input_event_tail = 0;
    input_event_queue_full = false;
}

/**
 * Submit key press event
 */
void input_key_press(input_device_t *dev, uint32_t keycode, uint8_t modifiers) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_KEY_PRESS;
    event.key.keycode = keycode;
    event.key.modifiers = modifiers;
    input_event_submit(dev, &event);
}

/**
 * Submit key release event
 */
void input_key_release(input_device_t *dev, uint32_t keycode) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_KEY_RELEASE;
    event.key.keycode = keycode;
    input_event_submit(dev, &event);
}

/**
 * Submit key character event
 */
void input_key_char(input_device_t *dev, char ch) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_KEY_CHAR;
    event.key.ascii = ch;
    input_event_submit(dev, &event);
}

/**
 * Submit mouse move event
 */
void input_mouse_move(input_device_t *dev, int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_MOUSE_MOVE;
    event.mouse.x = x;
    event.mouse.y = y;
    event.mouse.dx = dx;
    event.mouse.dy = dy;
    event.mouse.buttons = buttons;
    input_event_submit(dev, &event);
}

/**
 * Submit mouse button event
 */
void input_mouse_button(input_device_t *dev, uint8_t button, bool pressed) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_MOUSE_BUTTON;
    event.mouse.buttons = button;
    input_event_submit(dev, &event);
}

/**
 * Submit mouse scroll event
 */
void input_mouse_scroll(input_device_t *dev, int32_t delta) {
    input_event_t event;
    k_memset(&event, 0, sizeof(event));
    event.type = INPUT_EVENT_MOUSE_SCROLL;
    event.scroll.delta = delta;
    input_event_submit(dev, &event);
}

