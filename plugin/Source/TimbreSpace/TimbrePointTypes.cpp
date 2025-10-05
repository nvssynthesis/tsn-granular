/*
  ==============================================================================

    TimbrePointTypes.cpp
    Created: 4 Jul 2025 8:32:06pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbrePointTypes.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"

namespace nvs::timbrespace {

std::array<juce::uint8, 3> toUnsigned(Timbre3DPoint p3D) {
	for (auto p : p3D){
		assert(inRangeM1_1(p));
	}
	using namespace nvs::memoryless;
	std::array<juce::uint8, 3> u {
		static_cast<juce::uint8>(biuni(p3D[0]) * 255.f),
		static_cast<juce::uint8>(biuni(p3D[1]) * 255.f),
		static_cast<juce::uint8>(biuni(p3D[2]) * 255.f)
	};
	return u;
}

}
