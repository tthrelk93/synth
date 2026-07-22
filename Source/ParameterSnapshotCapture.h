#pragma once

namespace ParameterSnapshotCapture
{
template <typename Snapshot, typename GenerationReader, typename ValueReader,
          typename ContractReader, typename Builder>
Snapshot capture (const Snapshot& fallback,
                  const Snapshot& initial,
                  GenerationReader&& readGeneration,
                  ValueReader&& readValues,
                  ContractReader&& readContract,
                  Builder&& build) noexcept
{
    constexpr int maximumAttempts = 3;
    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        const auto before = readGeneration();
        if ((before & 1u) != 0u)
            continue;
        const auto values = readValues();
        const auto contract = readContract();
        const auto after = readGeneration();
        if (before == after && (after & 1u) == 0u)
            return build (values, contract, after);
    }

    auto result = fallback.coherent ? fallback : initial;
    result.usedFallback = true;
    return result;
}
}
