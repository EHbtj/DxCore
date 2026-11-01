// Also compile on DU/DA/DB/DD without native USB.
#include <UART_swap.h>

// Constructors and swap() take a table row, not shifted PORTMUX register bits.
#if defined(__AVR_DU__)
static_assert(HWSERIAL1_MUX_DEFAULT == 2, "DU Serial1 must default to PD6/PD7");
#endif
#if defined(USART0)
static_assert(HWSERIAL0_MUX_DEFAULT < MUXCOUNT_USART0, "Serial0 default is outside the pin table");
#endif
#if defined(USART1)
static_assert(HWSERIAL1_MUX_DEFAULT < MUXCOUNT_USART1, "Serial1 default is outside the pin table");
#endif
#if defined(USART2)
static_assert(HWSERIAL2_MUX_DEFAULT < MUXCOUNT_USART2, "Serial2 default is outside the pin table");
#endif
#if defined(USART3)
static_assert(HWSERIAL3_MUX_DEFAULT < MUXCOUNT_USART3, "Serial3 default is outside the pin table");
#endif
#if defined(USART4)
static_assert(HWSERIAL4_MUX_DEFAULT < MUXCOUNT_USART4, "Serial4 default is outside the pin table");
#endif
#if defined(USART5)
static_assert(HWSERIAL5_MUX_DEFAULT < MUXCOUNT_USART5, "Serial5 default is outside the pin table");
#endif

HardwareSerial &legacy = Serial;
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  legacy.begin(115200);
  Serial1.begin(115200);
}
void loop() {
  if (legacy.available()) Serial1.write((uint8_t)legacy.read());
  #if !defined(MILLIS_USE_TIMERNONE)
    digitalWrite(LED_BUILTIN, (millis() / 1000) & 1);
  #endif
}
