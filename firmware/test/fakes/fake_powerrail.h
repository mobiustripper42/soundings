#pragma once
#include "ipowerrail.h"

namespace soundings {

// FakePowerRail — host-test stand-in for the switched sensor rail. Records both the
// current state and the transition count, because the two failures worth catching are
// different: a rail left ON after a read is the one that flattens the pack (DEC-006's
// finding is that the battery killer is a node stuck awake), and a rail toggled twice in
// one cycle means the driver restarted the sensor mid-read and its discard count is
// meaningless.
class FakePowerRail : public IPowerRail {
public:
    void on()  override { if (!state_) ++onCount_;  state_ = true;  }
    void off() override { if (state_)  ++offCount_; state_ = false; }

    bool isOn()     const { return state_; }
    int  onCount()  const { return onCount_; }
    int  offCount() const { return offCount_; }

private:
    bool state_    = false;
    int  onCount_  = 0;
    int  offCount_ = 0;
};

} // namespace soundings
