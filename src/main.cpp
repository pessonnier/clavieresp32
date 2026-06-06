#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

#include "hid_config.h"

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

namespace {
constexpr int HID_MOUSE_MIN_DELTA = -127;
constexpr int HID_MOUSE_MAX_DELTA = 127;
constexpr int MOVE_STEP_DELAY_MS = 5;
constexpr int ACTION_DELAY_MS = 250;

bool demoAlreadyRun = false;

void moveRelativePixels(int deltaX, int deltaY) {
  while (deltaX != 0 || deltaY != 0) {
    const int stepX = constrain(deltaX, HID_MOUSE_MIN_DELTA, HID_MOUSE_MAX_DELTA);
    const int stepY = constrain(deltaY, HID_MOUSE_MIN_DELTA, HID_MOUSE_MAX_DELTA);

    Mouse.move(stepX, stepY);
    deltaX -= stepX;
    deltaY -= stepY;
    delay(MOVE_STEP_DELAY_MS);
  }
}

void movePointerToApproximateScreenCenter() {
  // Une souris HID classique envoie des déplacements relatifs et ne connaît pas
  // la résolution réelle de l'hôte. On force donc d'abord le pointeur vers le
  // coin supérieur gauche, puis on avance jusqu'au centre configuré.
  moveRelativePixels(-SCREEN_WIDTH_PX * 2, -SCREEN_HEIGHT_PX * 2);
  delay(ACTION_DELAY_MS);
  moveRelativePixels(SCREEN_WIDTH_PX / 2, SCREEN_HEIGHT_PX / 2);
}

void runKeyboardMouseDemo() {
  movePointerToApproximateScreenCenter();
  delay(ACTION_DELAY_MS);

  Mouse.click(MOUSE_LEFT);
  delay(ACTION_DELAY_MS);

  Keyboard.print("abcdef");
}
}  // namespace

void setup() {
  Serial.begin(115200);

  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  delay(USB_ENUMERATION_DELAY_MS);
}

void loop() {
  if (!demoAlreadyRun) {
    demoAlreadyRun = true;
    runKeyboardMouseDemo();
  }

  delay(1000);
}
