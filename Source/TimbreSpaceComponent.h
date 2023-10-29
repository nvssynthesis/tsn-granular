/*
  ==============================================================================

    TimbreSpaceComponent.h
    Created: 28 Oct 2023 4:24:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "nvs_libraries/include/nvs_memoryless.h"
#include "fmt/core.h"

static bool inRange0_1(float x){
	return ( (x >= 0.f) && (x <= 1.f) );
}
static bool inRangeM1_1(float x){
	return ( (x >= -1.f) && (x <= 1.f) );
}

class TimbreSpaceComponent	:	public juce::Component
{
	using point_t = juce::Point<float>;
	using timbre2DPoint = point_t;
	struct timbre5DPoint
	{
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
	TimbreSpaceComponent(){
		timbres5D.add({.p2D{juce::Point(0.f, 0.f)}, .p3D{1.f, 0.5f, 0.2f}});
		timbres5D.add({.p2D{0.5f, 0.5f}, .p3D{0.f, 0.8f, 0.9f}});
	}
	void add2DPoint(float x, float y){
		point_t p(x, y);
		add2DPoint(p);
	}
	void add2DPoint(point_t p){
		assert(inRangeM1_1(p.getX()) && inRangeM1_1(p.getY()));
		timbres5D.add({
			.p2D{p},
			.p3D{1.f, 1.f, 1.f}
		});
	}
	void add5DPoint(point_t p2D, std::array<float, 3> p3D){
		assert(inRangeM1_1(p2D.getX()) && inRangeM1_1(p2D.getY())
			   && inRangeM1_1(p3D[0]) && inRangeM1_1(p3D[1]) && inRangeM1_1(p3D[2])
			   );
		timbres5D.add({
			.p2D{p2D},
			.p3D{p3D}
		});
	}
	void add5DPoint(float x, float y, float z, float w, float v){
		point_t p2D(x,y);
		std::array<float, 3> p3D{z, w, v};
		add5DPoint(p2D, p3D);
	}
	void paint(juce::Graphics &g) override {
		g.setOpacity(0.75f);
		g.fillAll(juce::Colours::purple);
		
		juce::Rectangle<float> r = g.getClipBounds().toFloat();
		auto const w = r.getWidth();
		auto const h = r.getHeight();
		g.setColour(juce::Colours::blue);
		
		auto pointToRect = [](point_t p) -> juce::Rectangle<float> {
			point_t upperLeft{p}, bottomRight{p};
			constexpr float halfDotSize {2.f};
			upperLeft.addXY(-halfDotSize, -halfDotSize);
			bottomRight.addXY(halfDotSize, halfDotSize);
			return juce::Rectangle<float>(upperLeft, bottomRight);
		};
		auto transformFromZeroOrigin = [](point_t p) -> point_t {
//			input bounds:  ([-1..1], [-1..1])
//			output bounds: ([0..1], [0..1])
			p.addXY(1.f, 1.f);	// [0..2]
			p *= 0.5f;			// [-1..1]
			return p;
		};
		for (auto p5 : timbres5D){
			point_t p = p5.get2D();
			auto const p3 = p5.toUnsigned();
			g.setColour(juce::Colour(p3[0], p3[1], p3[2], 1.f));
			p = transformFromZeroOrigin(p);
			p *= point_t(w,h);
			auto rect = pointToRect(p);
			g.fillEllipse(rect);
		}
	}
	void resized() override {}

	int findNearestPoint(float x, float y){
		float diff = 1e15;
		int idx = 0;
		for (int i = 0; i < timbres5D.size(); ++i){
			auto p = timbres5D[i].get2D();
			auto xx = (p.getX() - x);
			auto yy = (p.getY() - y);
			auto tmp = xx*xx + yy*yy;
			if (tmp < diff){
				diff = tmp;
				idx = i;
			}
		}
		return idx;
	}
	void mouseDown (const juce::MouseEvent &event) override {
		[[maybe_unused]] float x = static_cast<float>(event.getMouseDownX());
		[[maybe_unused]] float y = static_cast<float>(event.getMouseDownY());
		auto bounds = getLocalBounds().toFloat();
		x /= bounds.getWidth();
		y /= bounds.getHeight();
		using namespace nvs::memoryless;
		x = unibi(x);
		y = unibi(y);
		fmt::print("in mouseDown at {}, {}\n", x, y);
		int nearestIdx = findNearestPoint(x, y);
		auto nearestPoint = timbres5D[nearestIdx].get2D();
		fmt::print("nearestIdx: {}, point x,y: {},{}\n",
				   nearestIdx, nearestPoint.getX(), nearestPoint.getY());
	}
private:
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
