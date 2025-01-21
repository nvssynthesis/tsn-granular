/*
  ==============================================================================

    TimbreSpaceComponent.h
    Created: 28 Oct 2023 4:24:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "fmt/core.h"

/**
 TODO:
 -use legitimate 5D (or N-D) point class without mismatched smaller dimension subtypes. will need this to perform e.g. rotations
*/

namespace {
constexpr bool inRange0_1(float x){
	return ( (x >= 0.f) && (x <= 1.f) );
}
constexpr bool inRangeM1_1(float x){
	return ( (x >= -1.f) && (x <= 1.f) );
}
}
class TimbreSpaceComponent	:	public juce::Component
{
public:
	using timbre2DPoint = juce::Point<float>;
	using timbre3DPoint = std::array<float, 3>;
	struct timbre5DPoint {
		timbre2DPoint _p2D;			// used to locate the point in x,y plane
		timbre3DPoint _p3D;	// used to describe the colour (hsv)

		bool operator==(timbre5DPoint const &other) const;
		timbre2DPoint get2D() const { return _p2D; }
		timbre3DPoint get3D() const { return _p3D; }

		// to easily trade hsv for rbg
		std::array<juce::uint8, 3> toUnsigned() const;
	};
	TimbreSpaceComponent() = default;
	void add5DPoint(timbre2DPoint p2D, timbre3DPoint p3D);
	void add5DPoint(float x, float y, float z, float w, float v);
	void clear();
	void paint(juce::Graphics &g) override;
	void resized() override;

	
	void setCurrentPointFromNearest(juce::Point<float> point, bool verbose=false);
	void mouseDown (const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
	int getCurrentPointIdx() const;
private:
	void add2DPoint(float x, float y);
	void add2DPoint(timbre2DPoint p);
	
	juce::Array<timbre5DPoint> timbres5D;
	int currentPointIdx {0};
	
	juce::Point<float> normalizePosition_neg1_pos1(juce::Point<int> pos);
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
