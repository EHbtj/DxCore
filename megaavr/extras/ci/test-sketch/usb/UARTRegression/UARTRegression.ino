// Also compile on DU/DA/DB/DD without native USB.
HardwareSerial &legacy = Serial;
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  legacy.begin(115200);
  Serial1.begin(115200);
}
void loop() {
  if (legacy.available()) Serial1.write((uint8_t)legacy.read());
  digitalWrite(LED_BUILTIN, (millis() / 1000) & 1);
}
