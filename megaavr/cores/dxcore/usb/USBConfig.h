/* USB identifiers for the AVR DU evaluation board. */
#pragma once

#ifndef USB_VID
  #define USB_VID 0x04D8
#endif
#ifndef USB_PID
  #define USB_PID 0x0B15
#endif
#ifndef USB_MANUFACTURER
  #define USB_MANUFACTURER "Microchip Technology Inc."
#endif
#ifndef USB_PRODUCT
  #define USB_PRODUCT "DxCore AVR64DU32 CDC"
#endif

/* Bounded foreground waits; independent of the selected millis() timer. */
#ifndef USB_TX_TIMEOUT_MS
  #define USB_TX_TIMEOUT_MS 100
#endif
