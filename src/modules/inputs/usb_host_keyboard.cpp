#include "modules/usb_host_keyboard.h"

#include <globals.h>

#if defined(USB_HOST_KEYBOARD)

#include <Arduino.h>
#include <pgmspace.h>
#include <cstring>

#include "Bad_Usb_Lib/KeyboardLayout.h"
#include "Bad_Usb_Lib/keys.h"

extern const uint8_t KeyboardLayout_en_US[128];

#include <soc/soc_caps.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "class/hid/hid.h"
#include "class/hid/hid_host.h"
#include "tusb.h"

#if SOC_USB_OTG_SUPPORTED

namespace {

constexpr char const *TAG = "USBHostKeyboard";

TaskHandle_t hostTaskHandle = nullptr;
bool stackStarted = false;
bool keyboardMounted = false;
hid_keyboard_report_t previousReport = {0};

bool keyWasPressed(uint8_t keycode, hid_keyboard_report_t const &report) {
    for (uint8_t idx : report.keycode) {
        if (idx == keycode) {
            return true;
        }
    }
    return false;
}

char hidToAscii(uint8_t keycode, uint8_t modifiers) {
    bool shift = modifiers & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
    bool altGr = modifiers & KEYBOARD_MODIFIER_RIGHTALT;

    for (size_t ascii = 0; ascii < 128; ++ascii) {
        uint8_t entry = pgm_read_byte(&KeyboardLayout_en_US[ascii]);
        if (entry == 0) {
            continue;
        }

        bool entryShift = entry & SHIFT;
        bool entryAltGr = (entry & ALT_GR) == ALT_GR;
        uint8_t hid = entry & 0x3F;

        if (hid == keycode) {
            if (entryAltGr && !altGr) {
                continue;
            }

            if (!entryAltGr && altGr) {
                continue;
            }

            if ((entryShift != 0) == shift) {
                return static_cast<char>(ascii);
            }
        }
    }
    return 0;
}

void pushKeypress(uint8_t keycode, uint8_t modifiers) {
    KeyStroke.pressed = true;
    KeyStroke.modifiers = modifiers;
    KeyStroke.hid_keys.push_back(keycode);

    bool ctrl = modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
    bool alt = modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);
    bool gui = modifiers & (KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_RIGHTGUI);

    if (ctrl) KeyStroke.ctrl = true;
    if (alt) KeyStroke.alt = true;
    if (gui) KeyStroke.gui = true;

    switch (keycode) {
        case HID_KEY_ENTER:
            KeyStroke.enter = true;
            SelPress = true;
            break;
        case HID_KEY_ESCAPE:
            KeyStroke.exit_key = true;
            EscPress = true;
            break;
        case HID_KEY_BACKSPACE:
            KeyStroke.del = true;
            break;
        case HID_KEY_ARROW_RIGHT:
            NextPress = true;
            break;
        case HID_KEY_ARROW_LEFT:
            PrevPress = true;
            break;
        case HID_KEY_ARROW_UP:
            UpPress = true;
            break;
        case HID_KEY_ARROW_DOWN:
            DownPress = true;
            break;
        case HID_KEY_PAGE_UP:
            PrevPagePress = true;
            break;
        case HID_KEY_PAGE_DOWN:
            NextPagePress = true;
            break;
        default: {
            char ascii = hidToAscii(keycode, modifiers);
            if (ascii != 0) {
                KeyStroke.word.push_back(ascii);
            }
            break;
        }
    }

    AnyKeyPress = true;
}

void processKeyboardReport(hid_keyboard_report_t const &report) {
    for (uint8_t keycode : report.keycode) {
        if (keycode == 0) {
            continue;
        }

        if (!keyWasPressed(keycode, previousReport)) {
            pushKeypress(keycode, report.modifier);
        }
    }

    previousReport = report;
}

void usbHostKeyboardTask(void *param) {
    while (stackStarted) {
        tuh_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(nullptr);
}

} // namespace

extern "C" void tuh_hid_mount_cb(
    uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len
) {
    (void)desc_report;
    (void)desc_len;

    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboardMounted = true;
        tuh_hid_receive_report(dev_addr, instance);
        ESP_LOGI(TAG, "Keyboard mounted on address %u", dev_addr);
    }
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboardMounted = false;
        previousReport = {0};
        ESP_LOGI(TAG, "Keyboard removed from address %u", dev_addr);
    }
}

extern "C" void tuh_hid_report_received_cb(
    uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len
) {
    if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_KEYBOARD) {
        return;
    }

    if (len < sizeof(hid_keyboard_report_t)) {
        return;
    }

    hid_keyboard_report_t parsed{};
    memcpy(&parsed, report, sizeof(hid_keyboard_report_t));
    processKeyboardReport(parsed);
    tuh_hid_receive_report(dev_addr, instance);
}

void initUsbHostKeyboard() {
    if (stackStarted) {
        return;
    }

    previousReport = {0};

    static bool tusbInitDone = false;
    if (!tusbInitDone) {
        if (!tuh_inited()) {
            tuh_init(0);
        }
        tusbInitDone = true;
    }

    stackStarted = true;

    if (hostTaskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            usbHostKeyboardTask,
            "USBHostKeyboard",
            4096,
            nullptr,
            3,
            &hostTaskHandle,
            CONFIG_ARDUINO_RUNNING_CORE
        );
    }
}

void shutdownUsbHostKeyboard() {
    stackStarted = false;
    if (hostTaskHandle != nullptr) {
        TaskHandle_t handle = hostTaskHandle;
        hostTaskHandle = nullptr;
        vTaskDelete(handle);
    }
}

void pollUsbHostKeyboard() {
    // Polling is handled in the dedicated task.
    (void)0;
}

bool usbHostKeyboardConnected() { return keyboardMounted; }

#else

void initUsbHostKeyboard() {
    ESP_LOGW("USBHostKeyboard", "USB host keyboard requested but USB OTG is not supported on this target");
}
void shutdownUsbHostKeyboard() {}
void pollUsbHostKeyboard() {}
bool usbHostKeyboardConnected() { return false; }

#endif

#else

void initUsbHostKeyboard() {}
void shutdownUsbHostKeyboard() {}
void pollUsbHostKeyboard() {}
bool usbHostKeyboardConnected() { return false; }

#endif

