#include "Oscillator.h"

#include <cmath>
#include <iostream>

int main()
{
    Oscillator oscillator;
    oscillator.setSampleRate(48000.0f);
    oscillator.setRange(Oscillator::Eight);
    oscillator.setWaveform(Oscillator::Sin);
    oscillator.start(440.0f);

    double absoluteSum = 0.0;
    for (int sample = 0; sample < 480; ++sample)
        absoluteSum += std::abs(oscillator.processNextSample(0.0f, true));

    if (!std::isfinite(absoluteSum) || absoluteSum <= 0.0)
    {
        std::cerr << "Offline render foundation produced invalid output\n";
        return 1;
    }

    std::cout << "Rendered 480 ModelDCore samples\n";
    return 0;
}
