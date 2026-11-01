/* Compatibility placeholder for development-platform overlay updates.
 *
 * The USB descriptor implementation was renamed to descriptor.cpp. Keep this
 * intentionally empty file so copying a new platform over an older installation
 * replaces the obsolete CDC-only implementation. Otherwise Arduino compiles both
 * files and the old weak descriptor callback can silently hide the HID interfaces.
 * The implementation lives exclusively in descriptor.cpp.
 */
