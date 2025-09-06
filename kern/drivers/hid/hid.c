/* Aurora Human Interface Device (HID) Driver */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"
#include <stdio.h>

/* HID Report Types */
#define HID_REPORT_INPUT    0x01
#define HID_REPORT_OUTPUT   0x02
#define HID_REPORT_FEATURE  0x03

/* HID Usage Pages */
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01
#define HID_USAGE_PAGE_KEYBOARD         0x07
#define HID_USAGE_PAGE_LED              0x08
#define HID_USAGE_PAGE_BUTTON           0x09
#define HID_USAGE_PAGE_CONSUMER         0x0C

/* HID Usages - Generic Desktop */
#define HID_USAGE_POINTER               0x01
#define HID_USAGE_MOUSE                 0x02
#define HID_USAGE_JOYSTICK              0x04
#define HID_USAGE_GAMEPAD               0x05
#define HID_USAGE_KEYBOARD              0x06
#define HID_USAGE_KEYPAD                0x07
#define HID_USAGE_X                     0x30
#define HID_USAGE_Y                     0x31
#define HID_USAGE_Z                     0x32
#define HID_USAGE_WHEEL                 0x38

/* PS/2 Controller Ports */
#define PS2_DATA_PORT       0x60
#define PS2_STATUS_PORT     0x64
#define PS2_COMMAND_PORT    0x64

/* PS/2 Status Register Bits */
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_SYSTEM       0x04
#define PS2_STATUS_COMMAND      0x08
#define PS2_STATUS_TIMEOUT      0x40
#define PS2_STATUS_PARITY       0x80

/* PS/2 Commands */
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT2   0xA7
#define PS2_CMD_ENABLE_PORT2    0xA8
#define PS2_CMD_TEST_PORT2      0xA9
#define PS2_CMD_TEST_CTRL       0xAA
#define PS2_CMD_TEST_PORT1      0xAB
#define PS2_CMD_DISABLE_PORT1   0xAD
#define PS2_CMD_ENABLE_PORT1    0xAE
#define PS2_CMD_WRITE_PORT2     0xD4

/* Keyboard Scan Codes */
#define KEY_ESC             0x01
#define KEY_1               0x02
#define KEY_2               0x03
#define KEY_3               0x04
#define KEY_4               0x05
#define KEY_5               0x06
#define KEY_6               0x07
#define KEY_7               0x08
#define KEY_8               0x09
#define KEY_9               0x0A
#define KEY_0               0x0B
#define KEY_MINUS           0x0C
#define KEY_EQUAL           0x0D
#define KEY_BACKSPACE       0x0E
#define KEY_TAB             0x0F
#define KEY_Q               0x10
#define KEY_W               0x11
#define KEY_E               0x12
#define KEY_R               0x13
#define KEY_T               0x14
#define KEY_Y               0x15
#define KEY_U               0x16
#define KEY_I               0x17
#define KEY_O               0x18
#define KEY_P               0x19
#define KEY_LEFTBRACE       0x1A
#define KEY_RIGHTBRACE      0x1B
#define KEY_ENTER           0x1C
#define KEY_LEFTCTRL        0x1D
#define KEY_A               0x1E
#define KEY_S               0x1F
#define KEY_D               0x20
#define KEY_F               0x21
#define KEY_G               0x22
#define KEY_H               0x23
#define KEY_J               0x24
#define KEY_K               0x25
#define KEY_L               0x26
#define KEY_SEMICOLON       0x27
#define KEY_APOSTROPHE      0x28
#define KEY_GRAVE           0x29
#define KEY_LEFTSHIFT       0x2A
#define KEY_BACKSLASH       0x2B
#define KEY_Z               0x2C
#define KEY_X               0x2D
#define KEY_C               0x2E
#define KEY_V               0x2F
#define KEY_B               0x30
#define KEY_N               0x31
#define KEY_M               0x32
#define KEY_COMMA           0x33
#define KEY_DOT             0x34
#define KEY_SLASH           0x35
#define KEY_RIGHTSHIFT      0x36
#define KEY_KPASTERISK      0x37
#define KEY_LEFTALT         0x38
#define KEY_SPACE           0x39
#define KEY_CAPSLOCK        0x3A

/* HID Device Types */
typedef enum {
    HID_TYPE_KEYBOARD = 1,
    HID_TYPE_MOUSE,
    HID_TYPE_GAMEPAD,
    HID_TYPE_JOYSTICK,
    HID_TYPE_TOUCHPAD,
    HID_TYPE_GENERIC
} hid_device_type_t;

/* HID Input Event */
typedef struct {
    UINT32 type;        /* Event type */
    UINT32 code;        /* Key/button code */
    UINT32 value;       /* Value (pressed/released/axis) */
    UINT64 timestamp;   /* Event timestamp */
} hid_input_event_t;

/* HID Device Structure */
typedef struct _hid_device {
    hid_device_type_t type;
    UINT32 vendor_id;
    UINT32 product_id;
    UINT32 version;
    char name[64];
    
    /* Device capabilities */
    UINT32 key_count;
    UINT32 button_count;
    UINT32 axis_count;
    UINT32 led_count;
    
    /* Input buffer */
    hid_input_event_t* input_buffer;
    UINT32 buffer_size;
    UINT32 buffer_head;
    UINT32 buffer_tail;
    AURORA_SPINLOCK buffer_lock;
    
    /* Device state */
    UINT8 key_state[256];       /* Keyboard state */
    UINT8 button_state[32];     /* Button state */
    INT32 axis_state[16];       /* Axis state */
    UINT8 led_state[8];         /* LED state */
    
    /* PS/2 specific */
    UINT32 ps2_port;            /* 1 or 2 */
    UINT32 ps2_irq;
    
    /* USB specific */
    void* usb_device;
    UINT32 usb_interface;
    UINT32 usb_endpoint_in;
    UINT32 usb_endpoint_out;
    
    struct _hid_device* next;
} hid_device_t;

/* HID Driver Private Data */
typedef struct {
    AURORA_SPINLOCK lock;
    hid_device_t* devices;
    UINT32 device_count;
    
    /* PS/2 Controller State */
    UINT8 ps2_config;
    UINT32 ps2_port1_enabled;
    UINT32 ps2_port2_enabled;
    
    /* Event handlers */
    void (*key_handler)(UINT32 key, UINT32 pressed);
    void (*mouse_handler)(INT32 dx, INT32 dy, UINT32 buttons);
    void (*gamepad_handler)(UINT32 buttons, INT32* axes);
} hid_driver_priv_t;

static hid_driver_priv_t g_hid_driver;

/* PS/2 Controller Functions */
static UINT8 ps2_read_data(void) {
    // while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL));
    // return inb(PS2_DATA_PORT);
    return 0; // Placeholder
}

static void ps2_write_data(UINT8 data) {
    // while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL);
    // outb(PS2_DATA_PORT, data);
}

static void ps2_write_command(UINT8 command) {
    // while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL);
    // outb(PS2_COMMAND_PORT, command);
}

static void ps2_write_port2(UINT8 data) {
    ps2_write_command(PS2_CMD_WRITE_PORT2);
    ps2_write_data(data);
}

/* Keyboard Functions */
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static void keyboard_process_scancode(hid_device_t* kbd, UINT8 scancode) {
    UINT32 pressed = !(scancode & 0x80);
    UINT32 key = scancode & 0x7F;
    
    if (key < 256) {
        kbd->key_state[key] = pressed;
        
        /* Add to input buffer */
        AURORA_IRQL old_irql;
        AuroraAcquireSpinLock(&kbd->buffer_lock, &old_irql);
        
        UINT32 next_head = (kbd->buffer_head + 1) % kbd->buffer_size;
        if (next_head != kbd->buffer_tail) {
            hid_input_event_t* event = &kbd->input_buffer[kbd->buffer_head];
            event->type = HID_REPORT_INPUT;
            event->code = key;
            event->value = pressed;
            event->timestamp = AuroraGetSystemTime();
            kbd->buffer_head = next_head;
        }
        
        AuroraReleaseSpinLock(&kbd->buffer_lock, old_irql);
        
        /* Call handler if registered */
        if (g_hid_driver.key_handler) {
            g_hid_driver.key_handler(key, pressed);
        }
    }
}

/* Mouse Functions */
static void mouse_process_packet(hid_device_t* mouse, UINT8* packet) {
    /* Standard PS/2 mouse packet format */
    UINT8 flags = packet[0];
    INT8 dx = packet[1];
    INT8 dy = packet[2];
    
    /* Extract button states */
    UINT32 buttons = 0;
    if (flags & 0x01) buttons |= 0x01; /* Left button */
    if (flags & 0x02) buttons |= 0x02; /* Right button */
    if (flags & 0x04) buttons |= 0x04; /* Middle button */
    
    /* Update device state */
    mouse->button_state[0] = (buttons & 0x01) ? 1 : 0;
    mouse->button_state[1] = (buttons & 0x02) ? 1 : 0;
    mouse->button_state[2] = (buttons & 0x04) ? 1 : 0;
    mouse->axis_state[0] += dx;
    mouse->axis_state[1] += dy;
    
    /* Add to input buffer */
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&mouse->buffer_lock, &old_irql);
    
    UINT32 next_head = (mouse->buffer_head + 1) % mouse->buffer_size;
    if (next_head != mouse->buffer_tail) {
        hid_input_event_t* event = &mouse->input_buffer[mouse->buffer_head];
        event->type = HID_REPORT_INPUT;
        event->code = 0; /* Mouse movement */
        event->value = (dx << 16) | (dy & 0xFFFF);
        event->timestamp = AuroraGetSystemTime();
        mouse->buffer_head = next_head;
    }
    
    AuroraReleaseSpinLock(&mouse->buffer_lock, old_irql);
    
    /* Call handler if registered */
    if (g_hid_driver.mouse_handler) {
        g_hid_driver.mouse_handler(dx, dy, buttons);
    }
}

/* PS/2 Interrupt Handlers */
static void ps2_keyboard_irq_handler(void* context) {
    hid_device_t* kbd = (hid_device_t*)context;
    UINT8 scancode = ps2_read_data();
    keyboard_process_scancode(kbd, scancode);
}

static void ps2_mouse_irq_handler(void* context) {
    static UINT8 mouse_packet[4];
    static UINT32 packet_index = 0;
    
    hid_device_t* mouse = (hid_device_t*)context;
    UINT8 data = ps2_read_data();
    
    mouse_packet[packet_index++] = data;
    
    if (packet_index >= 3) {
        mouse_process_packet(mouse, mouse_packet);
        packet_index = 0;
    }
}

/* HID Device Creation */
static hid_device_t* hid_create_device(hid_device_type_t type, const char* name) {
    hid_device_t* hid_dev = (hid_device_t*)AuroraAllocateMemory(sizeof(hid_device_t));
    if (!hid_dev) return NULL;
    
    memset(hid_dev, 0, sizeof(hid_device_t));
    
    hid_dev->type = type;
    strncpy(hid_dev->name, name, sizeof(hid_dev->name) - 1);
    
    /* Allocate input buffer */
    hid_dev->buffer_size = 256;
    hid_dev->input_buffer = (hid_input_event_t*)AuroraAllocateMemory(
        sizeof(hid_input_event_t) * hid_dev->buffer_size);
    
    if (!hid_dev->input_buffer) {
        AuroraFreeMemory(hid_dev);
        return NULL;
    }
    
    AuroraInitializeSpinLock(&hid_dev->buffer_lock);
    
    /* Set device capabilities based on type */
    switch (type) {
        case HID_TYPE_KEYBOARD:
            hid_dev->key_count = 256;
            hid_dev->led_count = 3; /* Caps, Num, Scroll */
            break;
            
        case HID_TYPE_MOUSE:
            hid_dev->button_count = 5;
            hid_dev->axis_count = 3; /* X, Y, Wheel */
            break;
            
        case HID_TYPE_GAMEPAD:
            hid_dev->button_count = 16;
            hid_dev->axis_count = 8; /* 2 analog sticks + triggers */
            break;
            
        default:
            break;
    }
    
    return hid_dev;
}

/* PS/2 Device Initialization */
static aur_status_t ps2_init_keyboard(void) {
    hid_device_t* kbd = hid_create_device(HID_TYPE_KEYBOARD, "PS/2 Keyboard");
    if (!kbd) return AUR_ERR_NOMEM;
    
    kbd->ps2_port = 1;
    kbd->ps2_irq = 1;
    
    /* Reset keyboard */
    ps2_write_data(0xFF);
    UINT8 response = ps2_read_data();
    if (response != 0xFA) {
        AuroraFreeMemory(kbd->input_buffer);
        AuroraFreeMemory(kbd);
        return AUR_ERR_IO;
    }
    
    /* Set scan code set 2 */
    ps2_write_data(0xF0);
    ps2_read_data(); /* ACK */
    ps2_write_data(0x02);
    ps2_read_data(); /* ACK */
    
    /* Register IRQ handler */
    // aur_irq_register(kbd->ps2_irq, ps2_keyboard_irq_handler, kbd);
    
    /* Add to device list */
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_hid_driver.lock, &old_irql);
    kbd->next = g_hid_driver.devices;
    g_hid_driver.devices = kbd;
    g_hid_driver.device_count++;
    AuroraReleaseSpinLock(&g_hid_driver.lock, old_irql);
    
    return AUR_OK;
}

static aur_status_t ps2_init_mouse(void) {
    hid_device_t* mouse = hid_create_device(HID_TYPE_MOUSE, "PS/2 Mouse");
    if (!mouse) return AUR_ERR_NOMEM;
    
    mouse->ps2_port = 2;
    mouse->ps2_irq = 12;
    
    /* Reset mouse */
    ps2_write_port2(0xFF);
    UINT8 response = ps2_read_data();
    if (response != 0xFA) {
        AuroraFreeMemory(mouse->input_buffer);
        AuroraFreeMemory(mouse);
        return AUR_ERR_IO;
    }
    
    /* Enable data reporting */
    ps2_write_port2(0xF4);
    ps2_read_data(); /* ACK */
    
    /* Register IRQ handler */
    aur_irq_register(mouse->ps2_irq, ps2_mouse_irq_handler, mouse);
    
    /* Add to device list */
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_hid_driver.lock, &old_irql);
    mouse->next = g_hid_driver.devices;
    g_hid_driver.devices = mouse;
    g_hid_driver.device_count++;
    AuroraReleaseSpinLock(&g_hid_driver.lock, old_irql);
    
    return AUR_OK;
}

/* HID Device IOCTL Handler */
static aur_status_t hid_device_ioctl(aur_device_t* dev, UINT32 code, void* inout) {
    if (!dev || !dev->drvdata) return AUR_ERR_INVAL;
    
    hid_device_t* hid_dev = (hid_device_t*)dev->drvdata;
    
    switch (code) {
        case AUR_IOCTL_GET_HID_INFO: {
            aur_hid_info_t* info = (aur_hid_info_t*)inout;
            if (!info) return AUR_ERR_INVAL;
            
            // info->type = hid_dev->type;
            info->vendor_id = hid_dev->vendor_id;
            info->product_id = hid_dev->product_id;
            info->version = hid_dev->version;
            // strncpy(info->name, hid_dev->name, sizeof(info->name));
            // info->key_count = hid_dev->key_count;
            // info->button_count = hid_dev->button_count;
            // info->axis_count = hid_dev->axis_count;
            // info->led_count = hid_dev->led_count;
            return AUR_OK;
        }
        
        // case AUR_IOCTL_READ_HID_EVENT: {
        //     hid_input_event_t* event = (hid_input_event_t*)inout;
        //     if (!event) return AUR_ERR_INVAL;
        //     
        //     AURORA_IRQL old_irql;
        //     AuroraAcquireSpinLock(&hid_dev->buffer_lock, &old_irql);
        //     
        //     if (hid_dev->buffer_head == hid_dev->buffer_tail) {
        //         AuroraReleaseSpinLock(&hid_dev->buffer_lock, old_irql);
        //         return AUR_ERR_NOMEM;
        //     }
        //     
        //     *event = hid_dev->input_buffer[hid_dev->buffer_tail];
        //     hid_dev->buffer_tail = (hid_dev->buffer_tail + 1) % hid_dev->buffer_size;
        //     
        //     AuroraReleaseSpinLock(&hid_dev->buffer_lock, old_irql);
        //     return AUR_OK;
        // }
        
        case AUR_IOCTL_GET_KEY_STATE: {
            UINT8* key_state = (UINT8*)inout;
            if (!key_state) return AUR_ERR_INVAL;
            
            memcpy(key_state, hid_dev->key_state, sizeof(hid_dev->key_state));
            return AUR_OK;
        }
        
        case AUR_IOCTL_SET_LED_STATE: {
            UINT8* led_state = (UINT8*)inout;
            if (!led_state) return AUR_ERR_INVAL;
            
            memcpy(hid_dev->led_state, led_state, sizeof(hid_dev->led_state));
            
            /* Update hardware LEDs for PS/2 keyboard */
            if (hid_dev->type == HID_TYPE_KEYBOARD && hid_dev->ps2_port == 1) {
                UINT8 led_cmd = 0xED;
                UINT8 led_data = (led_state[0] & 0x01) |       /* Scroll Lock */
                                ((led_state[1] & 0x01) << 1) | /* Num Lock */
                                ((led_state[2] & 0x01) << 2);  /* Caps Lock */
                
                ps2_write_data(led_cmd);
                ps2_read_data(); /* ACK */
                ps2_write_data(led_data);
                ps2_read_data(); /* ACK */
            }
            
            return AUR_OK;
        }
        
        default:
            return AUR_ERR_UNSUPPORTED;
    }
}

/* HID Driver Operations */
static aur_status_t hid_driver_probe(aur_device_t* dev) {
    /* HID devices are created during initialization */
    return AUR_OK;
}

static void hid_driver_remove(aur_device_t* dev) {
    if (dev && dev->drvdata) {
        hid_device_t* hid_dev = (hid_device_t*)dev->drvdata;
        
        /* Unregister IRQ */
        if (hid_dev->ps2_irq) {
            // aur_irq_unregister(hid_dev->ps2_irq);
        }
        
        /* Free resources */
        if (hid_dev->input_buffer) {
            AuroraFreeMemory(hid_dev->input_buffer);
        }
        AuroraFreeMemory(hid_dev);
    }
}

/* HID Driver Structure */
static aur_driver_t g_hid_driver_struct = {
    .name = "hid",
    .class_id = AUR_CLASS_PCI,
    .probe = hid_driver_probe,
    .remove = hid_driver_remove,
    .next = NULL
};

/* PS/2 Controller Initialization */
static aur_status_t ps2_controller_init(void) {
    /* Disable both PS/2 ports */
    ps2_write_command(PS2_CMD_DISABLE_PORT1);
    ps2_write_command(PS2_CMD_DISABLE_PORT2);
    
    /* Flush output buffer */
    // while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
    //     inb(PS2_DATA_PORT);
    // }
    
    /* Read configuration */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    g_hid_driver.ps2_config = ps2_read_data();
    
    /* Modify configuration - enable interrupts */
    g_hid_driver.ps2_config |= 0x03; /* Enable port 1 and 2 interrupts */
    g_hid_driver.ps2_config &= ~0x40; /* Enable port 1 translation */
    
    /* Write configuration */
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(g_hid_driver.ps2_config);
    
    /* Test PS/2 controller */
    ps2_write_command(PS2_CMD_TEST_CTRL);
    UINT8 test_result = ps2_read_data();
    if (test_result != 0x55) {
        return AUR_ERR_IO;
    }
    
    /* Test port 1 */
    ps2_write_command(PS2_CMD_TEST_PORT1);
    test_result = ps2_read_data();
    if (test_result == 0x00) {
        g_hid_driver.ps2_port1_enabled = 1;
    }
    
    /* Test port 2 */
    ps2_write_command(PS2_CMD_TEST_PORT2);
    test_result = ps2_read_data();
    if (test_result == 0x00) {
        g_hid_driver.ps2_port2_enabled = 1;
    }
    
    /* Enable working ports */
    if (g_hid_driver.ps2_port1_enabled) {
        ps2_write_command(PS2_CMD_ENABLE_PORT1);
    }
    if (g_hid_driver.ps2_port2_enabled) {
        ps2_write_command(PS2_CMD_ENABLE_PORT2);
    }
    
    return AUR_OK;
}

/* HID Driver Initialization */
void HIDDriverInitialize(void) {
    /* Initialize HID driver private data */
    memset(&g_hid_driver, 0, sizeof(g_hid_driver));
    AuroraInitializeSpinLock(&g_hid_driver.lock);
    
    /* Register HID driver */
    aur_driver_register(&g_hid_driver_struct);
    
    /* Initialize PS/2 controller */
    if (ps2_controller_init() == AUR_OK) {
        /* Initialize PS/2 devices */
        if (g_hid_driver.ps2_port1_enabled) {
            ps2_init_keyboard();
        }
        if (g_hid_driver.ps2_port2_enabled) {
            ps2_init_mouse();
        }
    }
    
    /* Create Aurora devices for each HID device */
    hid_device_t* hid_dev = g_hid_driver.devices;
    UINT32 device_index = 0;
    
    while (hid_dev) {
        aur_device_t* aur_dev = (aur_device_t*)AuroraAllocateMemory(sizeof(aur_device_t));
        if (aur_dev) {
            memset(aur_dev, 0, sizeof(aur_device_t));
            
            // snprintf(aur_dev->name, sizeof(aur_dev->name), "hid%u", device_index++);
            strcpy(aur_dev->name, "hid_device");
            aur_dev->class_id = AUR_CLASS_PCI;
            aur_dev->vendor_id = hid_dev->vendor_id;
            aur_dev->device_id = hid_dev->product_id;
            aur_dev->state = AUR_DEV_STATE_REMOVED;
            aur_dev->drvdata = hid_dev;
            aur_dev->ioctl = hid_device_ioctl;
            aur_dev->driver = &g_hid_driver_struct;
            aur_dev->irq_line = hid_dev->ps2_irq;
            
            aur_device_add(aur_dev);
        }
        
        hid_dev = hid_dev->next;
    }
}

/* HID Utility Functions */
hid_device_t* hid_find_device(hid_device_type_t type) {
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_hid_driver.lock, &old_irql);
    
    hid_device_t* hid_dev = g_hid_driver.devices;
    while (hid_dev) {
        if (hid_dev->type == type) {
            AuroraReleaseSpinLock(&g_hid_driver.lock, old_irql);
            return hid_dev;
        }
        hid_dev = hid_dev->next;
    }
    
    AuroraReleaseSpinLock(&g_hid_driver.lock, old_irql);
    return NULL;
}

void hid_set_key_handler(void (*handler)(UINT32 key, UINT32 pressed)) {
    g_hid_driver.key_handler = handler;
}

void hid_set_mouse_handler(void (*handler)(INT32 dx, INT32 dy, UINT32 buttons)) {
    g_hid_driver.mouse_handler = handler;
}

void hid_set_gamepad_handler(void (*handler)(UINT32 buttons, INT32* axes)) {
    g_hid_driver.gamepad_handler = handler;
}

/* Legacy compatibility functions */
void HidDriverInject(UINT8 code) {
    /* Find keyboard device and inject scancode */
    hid_device_t* kbd = hid_find_device(HID_TYPE_KEYBOARD);
    if (kbd) {
        keyboard_process_scancode(kbd, code);
    }
}
