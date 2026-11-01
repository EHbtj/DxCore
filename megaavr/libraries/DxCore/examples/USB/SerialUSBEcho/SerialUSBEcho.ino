// Select AVR64DU32 Curiosity Nano (native USB CDC + euboot).
// Connect the TARGET USB connector, then select its COM port.
void setup() {
  SerialUSB.begin();  // Optional: the USB board starts CDC before setup().
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  while (SerialUSB.available() && SerialUSB.availableForWrite()) {
    int value = SerialUSB.read();
    SerialUSB.write((uint8_t)value);
    if (value == '\n') {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
}
