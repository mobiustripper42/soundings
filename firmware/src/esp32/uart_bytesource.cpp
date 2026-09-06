#include "uart_bytesource.h"
#include <Arduino.h>

namespace soundings {

void UartByteSource::begin() {
    // Serial1, not Serial. Serial is the CP2102 bridge on GPIO43/44 and is the only way
    // anyone sees what this node is doing; handing it to the sensor would trade the
    // debug path for a peripheral the S3 has three of.
    //
    // The S3's GPIO matrix routes any UART to any pin, so naming GPIO6 here is a pin
    // choice and not a peripheral choice — there is no "the UART pins" to be constrained
    // by, which is the ESP32-classic instinct that does not carry over.
    Serial1.begin(kSensorBaud, SERIAL_8N1, kPinSensorRx, -1);
}

bool UartByteSource::readByte(uint8_t& out) {
    // available() first: read() on an empty buffer returns -1, and -1 narrowed into a
    // uint8_t is 0xFF — the A02YYUW's frame header. A driver fed a stream of phantom
    // headers would resynchronise forever on frames that never existed.
    if (Serial1.available() <= 0) return false;
    out = (uint8_t)Serial1.read();
    return true;
}

} // namespace soundings
