#pragma once
#include <stdint.h>
#include "ibytesource.h"

namespace soundings {

// The A02YYUW's UART, behind IByteSource. The sensor free-runs 4-byte frames at 9600 8N1
// with no trigger (a02yyuw.h), so all the driver ever needs from the platform is a stream
// of bytes it can poll without blocking.
//
// GPIO6 — J3 pin 18 on the Wireless Stick Lite V3, chosen 2026-09-02 from the Rev 1.1
// datasheet's Table 2.2-2, where it is `GPIO6, ADC1_CH5, TOUCH6`: no committed function,
// not a strapping pin, not USB, not JTAG, not flash.
//
// ⚠ Do not check this against the Arduino variant header. That file's SCK/MOSI/MISO
// (36/35/37) collide with its own Vext, LED and ADC_Ctrl definitions, and none of its SPI
// values match the pins sx1262_radio.h actually drives. It is wrong about this board.
constexpr int8_t   kPinSensorRx = 6;
constexpr uint32_t kSensorBaud  = 9600;

class UartByteSource : public IByteSource {
public:
    // Opens Serial1 on kPinSensorRx. Separate from the constructor because a global
    // constructed before Arduino's init() would be configuring a peripheral that is not up
    // yet; setup() calls this.
    //
    // The TX pin is deliberately -1. The sensor's own RX line is a mode strap tied low at
    // the sensor end (HARDWARE_BUILD_PLAN.md §8), so the node never transmits and spending
    // a pin on a transmitter would be spending it on nothing.
    void begin();

    // Non-blocking, per ibytesource.h: false means nothing is available right now, which is
    // the ordinary state between frames, not an error and not end-of-stream.
    bool readByte(uint8_t& out) override;
};

} // namespace soundings
