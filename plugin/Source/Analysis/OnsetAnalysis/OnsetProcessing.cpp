//
// Created by Nicholas Solem on 10/12/25.
//

#include "OnsetProcessing.h"
#include <ranges>
#include "essentiamath.h"

namespace nvs::analysis {

void filterOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds, float minimumOnsetDeltaSeconds)
{
    assert( std::ranges::is_sorted(onsetsInSeconds) );
    {	// filter out onsets that exceed the file length (taking into account minimum onset delta)

        // starting at end, count onsets exceeding lengthInSeconds
        size_t numProperOnsets = onsetsInSeconds.size();
        for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
            if (*it > (lengthInSeconds - minimumOnsetDeltaSeconds)){
                --numProperOnsets;
            }
            else {	// since the vector is sorted, we know that there are no more exceeding the proper length
                break;
            }
        }
        onsetsInSeconds.resize(numProperOnsets);

    }
    // filter out redundant onsets (can be defined by onsets which are too close together)
    {
        const auto new_end = std::ranges::unique(onsetsInSeconds, [minimumOnsetDeltaSeconds](float a, float b){
            return (b - a) < minimumOnsetDeltaSeconds;
        }).begin();
        onsetsInSeconds.erase(new_end, onsetsInSeconds.end());	// erase from new end to original end
    }
}

void subdivideGap(std::vector<float> &onsets,
                         const size_t startIdx,
                         const size_t endIdx,
                         const int numOnsetsToInsert,
                         const double lengthInSeconds) {
    const float start = onsets[startIdx];
    const float end = [onsets, endIdx, lengthInSeconds]() {
        if (endIdx < onsets.size())
            return onsets[endIdx];
        assert (endIdx == onsets.size()); // why would we possibly get an end index even greater than the length of the vector?
        return static_cast<float>(lengthInSeconds);
    }();

    assert(start <= end);

    const auto newOnsets = std::views::iota(1, numOnsetsToInsert + 1)
                   | std::views::transform([=](const int i) {
                         return start + (end - start) * static_cast<float>(i) / (numOnsetsToInsert + 1);
                     });

    onsets.insert(onsets.begin() + startIdx + 1,
                  newOnsets.begin(),
                  newOnsets.end());
}

void forceMinimumOnsets(std::vector<float> &onsets, const int minOnsets, const double lengthInSeconds) {
    // Handle empty case
    if (onsets.empty()) {
        // Create evenly spaced onsets from 0.0 to 1.0 (assuming normalized)
        for (int i = 0; i < minOnsets; ++i) {
            onsets.push_back(static_cast<float>(i) / (minOnsets - 1));
        }
        return;
    }

    // If we already have the minimum number of onsets, just return
    if (onsets.size() >= minOnsets) {
        return;
    }

    // Find the largest gap and subdivide it repeatedly until we have minOnsets
    while (onsets.size() < minOnsets) {
        // Find the largest gap
        size_t largestGapIdx = 0;
        float largestGapSize = 0.0f;

        for (size_t i = 0; i < onsets.size() - 1; ++i) {
            if (const float gapSize = onsets[i + 1] - onsets[i];
                gapSize > largestGapSize)
            {
                largestGapSize = gapSize;
                largestGapIdx = i;
            }
        }

        // Insert one onset in the middle of the largest gap
        subdivideGap(onsets, largestGapIdx, largestGapIdx + 1, 1, lengthInSeconds);
    }
}

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds) {
    /// -get onsetDiffs (a.k.a. the event lengths)
    const std::vector<float> onsetDiffs = [onsets]() {
        std::vector<float> result;
        result.reserve(onsets.size());
        for (size_t i = 0; i < onsets.size() - 1; ++i) {
            result.push_back(onsets[i+1] - onsets[i]);
        }
        return result;
    }();

    // detect length outliers. method for now: IQR?
    const auto [lowerBound, upperBound] = [onsets]() {
        /// -take 25th percentile (Q1) and 75th percentile (Q3) of lengths
        const float Q1 = essentia::percentile(onsets, 25.f);
        const float Q3 = essentia::percentile(onsets, 75.f);
        /// -set IQR = Q3 - Q1
        const float IQR = Q3 - Q1;
        /// -set lowerBound = Q1 - 1.5*IQR
        /// --this could actually be unused; or, we could possibly use it to REDUCE the density of certain portions
        const float LB = Q1 - 1.5*IQR;
        /// -set upperBound = Q3 + 1.5*IQR
        const float UB = Q3 + 1.5*IQR;
        return std::make_pair(LB, UB);
    }();

    /// -take 50th percentile (median); set this to the targetDensity for too-sparse sections
    const float medianDiff = essentia::median(onsetDiffs);

    for (size_t i = 0; i < onsets.size() - 1; /* increment depends on current onset */) {
    /// -if the corresponding onsetDiff (length) exceeds upperBound:
        if (const float currentDiff = onsets[i+1] - onsets[i];
            currentDiff > upperBound)
        {
    /// --set numOnsetsToInsert = int(currentDiff / medianDiff) - 1
            const int numOnsetsToInsert = static_cast<int>(currentDiff / medianDiff) - 1;
    /// --use subdivideGap to insert numOnsetsToInsert - 1 new onsets between the current and next onset
            subdivideGap(onsets, i, i+1, numOnsetsToInsert, lengthInSeconds);

    /// --skip over newly inserted onsets
            i += numOnsetsToInsert + 1;
        } else {
            ++i;
        }
    }
}

void normalizeOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds) {
    std::ranges::transform(onsetsInSeconds,	// formerly terminated via begin() + numProperOnsets
                           onsetsInSeconds.begin(),
                           [lengthInSeconds](const double f)
                           {
                               const double res = f / lengthInSeconds;
                               assert(res <= 1.0);
                               return res;
                           });
}

void denormalizeOnsets(std::vector<float> &normalizedOnsets, const double lengthInSeconds) {
    std::ranges::transform(normalizedOnsets, normalizedOnsets.begin(),
                          [lengthInSeconds](const double f) {
                              return f * lengthInSeconds;
                          });
}


}
