/*
  ==============================================================================

    TimbreSpaceComponent.h
    Created: 28 Oct 2023 4:24:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

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
				assert(p >= 0.f);
				assert(p <= 1.f);
			}
			std::array<juce::uint8, 3> u {
				static_cast<juce::uint8>(p3D[0] * 255.f),
				static_cast<juce::uint8>(p3D[1] * 255.f),
				static_cast<juce::uint8>(p3D[2] * 255.f)
			};
			return u;
		}
	};
public:
	TimbreSpaceComponent(){
		timbres5D.add({.p2D{juce::Point(0.f, 0.f)}, .p3D{1.f, 0.5f, 0.2f}});
		timbres5D.add({.p2D{0.5f, 0.5f}, .p3D{0.f, 0.8f, 0.9f}});
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
			constexpr float dotSize {3.f};
			upperLeft.addXY(-dotSize, -dotSize);
			bottomRight.addXY(dotSize, dotSize);
			return juce::Rectangle<float>(upperLeft, bottomRight);
		};
		auto transformFromZeroOrigin = [](point_t p) -> point_t {
//			input bounds:  ([-1..1], [-1..1])
//			output bounds: ([0..1], [0..1])
			p.addXY(1.f, 1.f);	// [0..2]
			p *= 0.5f;			// [-1..1]
			return p;
		};
		auto transformToZeroOrigin = [](point_t p) -> point_t {
//			input bounds:  ([0..1], [0..1])
//			output bounds: ([-1..1], [-1..1])
			p *= 2.f;			// [0..2]
			p.addXY(-1.f, -1.f);// [-1..1]
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
	
	void resized() override {
		
	}
	void mouseMove (const juce::MouseEvent &event) override {
		
	}
	void mouseDown (const juce::MouseEvent &event) override {
		
	}
	
private:
	juce::Array<timbre5DPoint> timbres5D;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
