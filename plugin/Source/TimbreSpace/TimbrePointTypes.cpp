/*
  ==============================================================================

    TimbrePointTypes.cpp
    Created: 4 Jul 2025 8:32:06pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbrePointTypes.h"

namespace nvs::timbrespace {


// bipolar to unipolar (assuming input is [-1..1]
static constexpr float _biuni(float x) noexcept {  // avoid including memoryless
    assert ((-1.f <= x) && (x <= 1.f));
    return (x + 1.f) * 0.5f;
}

std::array<uint8, 3> toUnsigned(Timbre3DPoint p3D) {
	for (auto p : p3D){
		assert(inRangeM1_1(p));
	}

    return {
        static_cast<uint8>(_biuni(p3D[0]) * 255.f),
        static_cast<uint8>(_biuni(p3D[1]) * 255.f),
        static_cast<uint8>(_biuni(p3D[2]) * 255.f)
    };
}

}
