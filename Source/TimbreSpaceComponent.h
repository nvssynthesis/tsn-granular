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
public:
	TimbreSpaceComponent(){
		timbres2D.add(juce::Point(0.f, 0.f));
		timbres2D.add({0.5f, 0.5f});
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
		auto transformFromZeroOrigin = [w, h](point_t p) -> point_t {
//			input bounds:  ([-1..1], [-1..1])
//			output bounds: ([0..1], [0..1])
			p.addXY(1.f, 1.f);	// [0..2]
			p *= 0.5f;			// [-1..1]
			return p;
		};
		auto transformToZeroOrigin = [w, h](point_t p) -> point_t {
//			input bounds:  ([0..1], [0..1])
//			output bounds: ([-1..1], [-1..1])
			p *= 2.f;			// [0..2]
			p.addXY(-1.f, -1.f);// [-1..1]
			return p;
		};
		for (auto p : timbres2D){
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
	juce::Array<timbre2DPoint> timbres2D;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
