#ifndef SIGNALFILTER_H
#define SIGNALFILTER_H

#include "Config.h"
#include "Types.h"

class SignalFilter {
public:
    SignalFilter();
    void begin();
    FilteredData update(const MappedSensorData& mapped, float dt);
    void reset();

private:
    FilteredData state;
};

#endif // SIGNALFILTER_H
