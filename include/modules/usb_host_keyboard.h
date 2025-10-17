#pragma once

#include <stdbool.h>

void initUsbHostKeyboard();
void shutdownUsbHostKeyboard();
void pollUsbHostKeyboard();
bool usbHostKeyboardConnected();

