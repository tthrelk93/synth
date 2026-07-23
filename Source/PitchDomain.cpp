#include "PitchDomain.h"

#include <array>
#include <cmath>

namespace PitchDomain {
namespace {
template <typename T>
Checked<T> accepted (T value) noexcept { return { value, true }; }

template <typename T>
Checked<T> rejected() noexcept { return {}; }
}

Checked<Semitones> midiNote (const int note) noexcept
{
    return note >= 0 && note <= 127
             ? accepted (Semitones { static_cast<double> (note - 69) })
             : rejected<Semitones>();
}

Checked<RangeContribution> range (const int index) noexcept
{
    constexpr std::array musical { -24.0, -12.0, 0.0, 12.0, 24.0 };
    if (index == 0)
        return accepted (RangeContribution { RangeMode::lowFrequency, {} });
    if (index < 1 || index > 5)
        return rejected<RangeContribution>();
    return accepted (RangeContribution {
        RangeMode::musical, Semitones { musical[static_cast<size_t> (index - 1)] }
    });
}

Checked<Semitones> masterTune (const int index) noexcept
{
    return index >= 0 && index <= 10
             ? accepted (Semitones { 0.5 * static_cast<double> (index - 5) })
             : rejected<Semitones>();
}

Checked<Semitones> oscillatorOffset (const int index) noexcept
{
    return index >= 0 && index <= 16
             ? accepted (Semitones { static_cast<double> (index - 8) })
             : rejected<Semitones>();
}

Checked<Semitones> ratioToSemitones (const double ratio) noexcept
{
    if (! std::isfinite (ratio) || ratio <= 0.0)
        return rejected<Semitones>();
    const auto value = 12.0 * std::log2 (ratio);
    return std::isfinite (value) ? accepted (Semitones { value })
                                 : rejected<Semitones>();
}

Checked<Semitones> fromHertz (const double hertz) noexcept
{
    return ratioToSemitones (hertz / 440.0);
}

Checked<Hertz> toHertz (const Semitones coordinate) noexcept
{
    if (! std::isfinite (coordinate.value))
        return rejected<Hertz>();
    const auto value = 440.0 * std::exp2 (coordinate.value / 12.0);
    return std::isfinite (value) && value > 0.0
             ? accepted (Hertz { value }) : rejected<Hertz>();
}

Result compose (const Contributions& values) noexcept
{
    const std::array terms {
        values.note.value, values.range.value, values.masterTune.value,
        values.oscillatorOffset.value, values.pitchWheel.value,
        values.calibration.value, values.modulation.value
    };
    double coordinate = 0.0;
    for (const auto term : terms) {
        if (! std::isfinite (term))
            return {};
        coordinate += term;
    }
    const auto hertz = toHertz (Semitones { coordinate });
    return hertz.valid
             ? Result { Semitones { coordinate }, hertz.value, true }
             : Result {};
}

} // namespace PitchDomain
