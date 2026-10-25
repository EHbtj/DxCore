// USB CDC and the Nano debugger's UART are independent.
// Serial is still Serial0. Serial1 uses PD6/PD7 on this USB board.
void setup() {
  SerialUSB.begin(115200); // CDC baud is metadata, not the USB transfer speed.
  Serial1.begin(115200);
}

void loop() {
  while (SerialUSB.available() && Serial1.availableForWrite()) {
    Serial1.write((uint8_t)SerialUSB.read());
  }
  while (Serial1.available() && SerialUSB.availableForWrite()) {
    SerialUSB.write((uint8_t)Serial1.read());
  }
}
