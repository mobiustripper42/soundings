#pragma once

namespace soundings {

// IHealAttemptFlag — "this boot has already tried, and failed, to write a sensor's
// configuration EEPROM. Do not try again."
//
// The DS18B20 arrives at its 12-bit factory default and the driver rewrites it to 9-bit
// once, in EEPROM, on the first wake that meets it (issue #94). That write is the ONLY
// EEPROM write in a healthy sensor's life. On a part whose EEPROM will not take, there is
// nothing to stop the driver retrying on every wake — and the configuration register is
// rated 50,000 writes, so a fifteen-minute cadence would wear it out in about seventeen
// months, inside a sealed tank lid.
//
// ⚠ The bound is PER BOOT, not a count, and that is the whole trick: the driver confirms
// success (by power-cycling the rail and re-reading the config byte) rather than counting
// failures, so there is no tally to keep and no threshold to tune. One bool is enough.
// Worst case is one EEPROM write per ESP32 boot — battery swaps and reflashes — against
// the 50,000 rating.
//
// The ESP32 binding is RTC slow memory (`RTC_DATA_ATTR`), which survives deep sleep but
// not a power cycle — exactly the property ISeqStore relies on (iseqstore.h:14-16), and
// exactly right here too: a node that has just had its battery changed should get one
// fresh attempt at a sensor that may itself have been changed in the same service visit.
// The fake keeps it in RAM.
struct IHealAttemptFlag {
    virtual bool attempted() const = 0;
    virtual void markAttempted() = 0;
    virtual ~IHealAttemptFlag() = default;
};

} // namespace soundings
