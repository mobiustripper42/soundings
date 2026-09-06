#include "vext_rail.h"
#include <Arduino.h>
#include <driver/gpio.h>

namespace soundings {
namespace {

constexpr gpio_num_t kVextGpio = (gpio_num_t)kPinVextCtrl;

} // namespace

void VextRail::on() {
    // ⚠ ORDER IS LOAD-BEARING, and it is the opposite of what reads naturally. The pad may
    // be HELD from a previous off() — including across a deep sleep — and a held pad
    // ignores every write. driver/gpio.h:394-400 says to configure the pin to the level you
    // want BEFORE releasing the hold, because releasing it makes the pad follow whatever
    // the GPIO registers currently say. Release first and the pad passes through the
    // default input state on its way, which on this board is the rail flicking off and on.
    pinMode(kPinVextCtrl, OUTPUT);
    digitalWrite(kPinVextCtrl, LOW);       // active LOW: LOW is the rail ON
    gpio_hold_dis(kVextGpio);
}

void VextRail::off() {
    pinMode(kPinVextCtrl, OUTPUT);
    digitalWrite(kPinVextCtrl, HIGH);      // HIGH is the rail OFF

    // ⚠ NOT rtc_gpio_hold_en(). GPIO36 is not an RTC IO — soc_caps.h:199 sets
    // SOC_RTCIO_PIN_COUNT to 22, so RTC IO is GPIO0-21 and the RTC call returns an error
    // for this pin. The pad would go unheld, which looks exactly like success and is
    // invisible until someone puts a meter on the sleep current. Issue #71's original
    // acceptance criterion named the RTC call; this is the correction.
    //
    // Digital-pad hold does not survive deep sleep on its own either — driver/gpio.h:377
    // says gpio_deep_sleep_hold_en() is required alongside it. It is global rather than
    // per-pin, and idempotent, so calling it on every off() costs nothing.
    gpio_hold_en(kVextGpio);
    gpio_deep_sleep_hold_en();
}

} // namespace soundings
