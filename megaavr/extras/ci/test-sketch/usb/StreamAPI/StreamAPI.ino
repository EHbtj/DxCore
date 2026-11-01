// Compile contract for the native USB board; not an interactive demonstration.
static_assert(__is_base_of(Stream, SerialUSBClass), "CDC must implement Arduino Stream");
// The default is a pin-table row, while MUX_PINSWAP_2 holds shifted register bits.
static_assert(HWSERIAL1_MUX_DEFAULT == 2, "USB UART1 must default to PD6/PD7");

HardwareSerial &legacy = Serial;
HardwareSerial &physical = SERIAL_PORT_HARDWARE;
Stream &monitor = SERIAL_PORT_MONITOR;
Stream &virtualPort = SERIAL_PORT_USBVIRTUAL;

void setup() {
  SerialUSB.begin();
  SerialUSB.begin(115200);
  legacy.begin(115200);
  Serial1.begin(115200);
  SerialUSB.setTimeout(1);
  uint8_t buffer[8] = {};
  SerialUSB.readBytes(buffer, sizeof(buffer));
  SerialUSB.readBytesUntil('\n', (char *)buffer, sizeof(buffer));
  SerialUSB.parseInt();
  SerialUSB.print(F("USB"));
  SerialUSB.println(123);
  SerialUSB.write(buffer, sizeof(buffer));
  SerialUSB.write("USB");
  SerialUSB.flush();
  SerialUSB.end();
  SerialUSB.begin();
}

void loop() {
  if (SerialUSB && SerialUSB.available() && SerialUSB.availableForWrite()) {
    SerialUSB.peek();
    SerialUSB.write((uint8_t)SerialUSB.read());
  }
}
