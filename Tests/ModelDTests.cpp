#include "Oscillator.h"

#include <cmath>
#include <iostream>

int main()
{
    if (Oscillator::getRangeLabel(Oscillator::Eight) != "8")
    {
        std::cerr << "ModelDCore oscillator range-label smoke test failed\n";
        return 1;
    }

    Oscillator oscillator;
    oscillator.setSampleRate(48000.0f);
    oscillator.setRange(Oscillator::Eight);
    oscillator.setWaveform(Oscillator::Sin);
    oscillator.start(440.0f);

    const auto sample = oscillator.processNextSample(0.0f, true);
    if (!std::isfinite(sample))
    {
        std::cerr << "ModelDCore oscillator generated a non-finite sample\n";
        return 1;
    }

    return 0;
}
