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
	using point_t = juce::Point<float>;
	using timbre2DPoint = point_t;
private:
	struct timbre5DPoint {
		timbre2DPoint p2D;			// used to locate the point in x,y plane
		std::array<float, 3> p3D;	// used to describe the colour (hsv)

		point_t get2D() const {
			return p2D;
		}
		std::array<float, 3> get3D() const {
			return p3D;
		}

		// to easily trade hsv for rbg
		std::array<juce::uint8, 3> toUnsigned() const {
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
	 };
	juce::Array<timbre5DPoint> timbres5D;
public:
	TimbreSpaceComponent() = default;
	void add5DPoint(point_t p2D, std::array<float, 3> p3D);
	void add5DPoint(float x, float y, float z, float w, float v);
	void clear();
	void paint(juce::Graphics &g) override;
	void resized() override;

	int findNearestPoint(float x, float y);
	
	void setCurrentPointFromNearest(juce::Point<float> point, bool verbose=false);
	void mouseDown (const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
	int getCurrentPointIdx() const;
private:
	void add2DPoint(float x, float y);
	void add2DPoint(point_t p);
	int currentPointIdx {0};
	juce::Point<float> normalizePosition_neg1_pos1(juce::Point<int> pos);
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
