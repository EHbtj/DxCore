// Compile-only API contract; use HIDKeyboardMouse for an interactive example.
#include <Keyboard.h>
#include <Mouse.h>

static_assert(__is_base_of(Print, Keyboard_), "Keyboard must implement Print");
static_assert(KEY_LEFT_CTRL == 0x80 && KEY_F24 == 0xFB, "Arduino key encoding");
static_assert(MOUSE_ALL == (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE), "Button masks");

void setup() {
  SerialUSB.begin(115200);
  Serial.begin(115200);
  Keyboard.begin();
  Mouse.begin();
}
void loop() {
  // Exercise every API without producing input until commanded.
  if (SerialUSB.read() != 't') {
    return;
  }
  if (Keyboard && Mouse) {
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press('a');
    Keyboard.release('a');
    Keyboard.releaseAll();
    Keyboard.write('x');
    Keyboard.write("USB");
    const uint8_t text[] = {'a', 'b'};
    Keyboard.write(text, sizeof(text));
    Keyboard.print(F("Flash"));
    Keyboard.println("Print");
    SerialUSB.println(Keyboard.getLeds());
    SerialUSB.println(Keyboard.getWriteError());
    Keyboard.clearWriteError();
    Mouse.move(-127, 127, -1);
    Mouse.press(MOUSE_RIGHT);
    SerialUSB.println(Mouse.isPressed(MOUSE_RIGHT));
    Mouse.release(MOUSE_ALL);
    Mouse.click(MOUSE_MIDDLE);
  }
  Keyboard.end();
  Mouse.end();
}
