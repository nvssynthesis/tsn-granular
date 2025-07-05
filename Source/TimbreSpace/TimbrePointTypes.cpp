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
//===============================================timbre5DPoint=================================================
bool Timbre5DPoint::operator==(Timbre5DPoint const &other) const {
	if (other.get2D() != _p2D){
		return false;
	}
	auto const other3 = other.get3D();
	for (size_t i = 0; i < _p3D.size(); ++i){
		if (_p3D[i] != other3[i]){
			return false;
		}
	}
	return true;
}

std::array<juce::uint8, 3> Timbre5DPoint::toUnsigned() const {
	for (auto p : _p3D){
		assert(inRangeM1_1(p));
	}
	using namespace nvs::memoryless;
	std::array<juce::uint8, 3> u {
		static_cast<juce::uint8>(biuni(_p3D[0]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[1]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[2]) * 255.f)
	};
	return u;
}
}
