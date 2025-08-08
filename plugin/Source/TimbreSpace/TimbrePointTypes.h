/*
  ==============================================================================

    TimbrePointTypes.h
    Created: 4 Jul 2025 8:32:06pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

namespace nvs::timbrespace {

using timbre2DPoint = juce::Point<float>;
using timbre3DPoint = std::array<float, 3>;

struct Timbre5DPoint {
	
	timbre2DPoint _p2D;			// used to locate the point in x,y plane
	timbre3DPoint _p3D;	// used to describe the colour (hsv)
	
	bool operator==(Timbre5DPoint const &other) const;
	timbre2DPoint get2D() const { return _p2D; }
	timbre3DPoint get3D() const { return _p3D; }
	
	// to easily trade hsv for rbg
	std::array<juce::uint8, 3> toUnsigned() const;
};

static constexpr bool inRange0_1(float x){
	return ( (x >= 0.f) && (x <= 1.f) );
}
static constexpr bool inRangeM1_1(float x){
	return ( (x >= -1.f) && (x <= 1.f) );
}

}	// namespace nvs::timbrespace
