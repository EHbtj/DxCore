// Select an AVR DU native USB board and Tools > USB Mode > CDC + Keyboard + Mouse.
// Open Serial Monitor on the target USB port (not the Nano debugger port).
// Send k: type a line, m: move right, c: left click, r: release all keys/buttons.
// No keyboard or mouse input is generated until a command is received.
#include <Keyboard.h>
#include <Mouse.h>

void setup() {
  SerialUSB.begin();
  Keyboard.begin();
  Mouse.begin();
}

void loop() {
  switch (SerialUSB.read()) {
    case 'k':
      Keyboard.println("Hello AVR DU!");
      break;
    case 'm':
      Mouse.move(10, 0);
      break;
    case 'c':
      Mouse.click();
      break;
    case 'r':
      Keyboard.releaseAll();
      Mouse.release(MOUSE_ALL);
      break;
  }
}
