#pragma once
#include "isensor.h"

namespace soundings {

// FakeSoilMoisture — host-test stand-in for the Watermark adapter. The test sets the raw
// reading it returns and whether the read "succeeds"; swapping it for the real AC-
// excitation driver at the bench is the entire point of the seam.
class FakeSoilMoisture : public ISoilMoisture {
public:
    void setReading(uint16_t raw, bool ok = true) { raw_ = raw; ok_ = ok; }
    Reading read() override { return {raw_, ok_}; }
private:
    uint16_t raw_ = 0;
    bool     ok_  = true;
};

} // namespace soundings
