#pragma once

namespace PitchDomain {

template <typename T>
struct Checked {
    T value {};
    bool valid = false;
};

struct Semitones { double value = 0.0; };
struct Hertz { double value = 440.0; };

enum class RangeMode { lowFrequency, musical };

enum class CalibrationProfile { baseline };

struct RangeContribution {
    RangeMode mode = RangeMode::musical;
    Semitones semitones {};
};

struct Contributions {
    Semitones note;
    Semitones range;
    Semitones masterTune;
    Semitones oscillatorOffset;
    Semitones pitchWheel;
    Semitones calibration;
    Semitones modulation;
};

struct Result {
    Semitones coordinate {};
    Hertz hertz {};
    bool valid = false;
};

Checked<Semitones> midiNote (int note) noexcept;
Checked<RangeContribution> range (int index) noexcept;
Checked<Semitones> masterTune (int index) noexcept;
Checked<Semitones> oscillatorOffset (int index) noexcept;
Checked<Semitones> pitchWheel (double normalizedValue) noexcept;
Checked<Semitones> calibration (
    CalibrationProfile profile,
    RangeContribution selectedRange) noexcept;
Checked<Semitones> ratioToSemitones (double ratio) noexcept;
Checked<Semitones> fromHertz (double hertz) noexcept;
Checked<Hertz> toHertz (Semitones coordinate) noexcept;
Result compose (const Contributions&) noexcept;

} // namespace PitchDomain
