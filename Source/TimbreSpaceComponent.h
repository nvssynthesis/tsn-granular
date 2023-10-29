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
	using timbre2DPoint = juce::Point<float>;
public:
	TimbreSpaceComponent(){
		timbres2D.add(juce::Point(0.f, 0.f));
		timbres2D.add({0.5f, 0.5f});
	}
	
	void paint(juce::Graphics &g) override {
		g.fillAll(juce::Colours::purple);
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
