// CircularBuffer.h
#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <JuceHeader.h>
#include <vector>

class CircularBuffer {
public:
    explicit CircularBuffer(int size);
    void write(float leftSample, float rightSample);
    bool read(float* leftOutput, float* rightOutput, int numSamples);

private:
    std::vector<float> buffer;
    int size;
    int writeIndex;
    int readIndex;
    juce::SpinLock mutex;
};

#endif // CIRCULARBUFFER_H
