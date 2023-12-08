#include "CircularBuffer.h"

CircularBuffer::CircularBuffer(int size)
    : buffer(size * 2, 0.0f), // Double the size for two channels
      size(size * 2), writeIndex(0), readIndex(0) {
    // Other initialization
}

void CircularBuffer::write(float leftSample, float rightSample) {
    const std::lock_guard<std::mutex> lock(mutex);
    buffer[writeIndex] = leftSample;
    buffer[(writeIndex + 1) % size] = rightSample;
    writeIndex = (writeIndex + 2) % size; // Increment by 2 for stereo
}

bool CircularBuffer::read(float* leftOutput, float* rightOutput, int numSamples) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (numSamples * 2 > size) return false; // Check for stereo buffer length
    for (int i = 0; i < numSamples; ++i) {
        int index = (readIndex + i * 2) % size;
        leftOutput[i] = buffer[index];
        rightOutput[i] = buffer[(index + 1) % size];
    }
    readIndex = (readIndex + numSamples * 2) % size;
    return true;
}
