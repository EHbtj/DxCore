/* USB identifiers and VUSB power configuration for AVR DU boards. */
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
  #if defined(DXCORE_USB_HID)
    #define USB_PRODUCT "DxCore AVR DU CDC + HID"
  #else
    #define USB_PRODUCT "DxCore AVR DU CDC"
  #endif
#endif

#ifndef DXCORE_USB_VREG
  #define DXCORE_USB_VREG 1
#endif
#if DXCORE_USB_VREG != 0 && DXCORE_USB_VREG != 1
  #error "DXCORE_USB_VREG must be 1 (internal regulator) or 0 (external 3.3 V VUSB)."
#endif

/* Bounded foreground waits; independent of the selected millis() timer. */
#ifndef USB_TX_TIMEOUT_MS
  #define USB_TX_TIMEOUT_MS 100
#endif
