/*
  ==============================================================================

    NonAutomatableTitledSlider.h
    Created: 10 Apr 2024 5:44:02pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

struct NonAutomatableTitledSlider : public juce::Component
{
	using Slider = juce::Slider;
	using Label = juce::Label;
	
	NonAutomatableTitledSlider(Slider::SliderStyle sliderStyle,
							juce::Slider::Listener *listener,
							   juce::String name,
				   juce::Slider::TextEntryBoxPosition entryPos = juce::Slider::TextBoxBelow)
	:
	_slider(),
	_label(name, name)
	{
		_slider.setSliderStyle(sliderStyle);
		_slider.setTextBoxStyle(entryPos, false, 50, 25);
		_slider.setNumDecimalPlacesToDisplay(2);
		
		
		_slider.setColour(Slider::ColourIds::thumbColourId, juce::Colours::palevioletred);
		_slider.setColour(Slider::ColourIds::textBoxTextColourId, juce::Colours::lightgrey);

		_slider.setName(name);
		_slider.setTitle(name);

		_slider.addListener(listener);
//		_label.setBoundsToFit(<#Rectangle<int> targetArea#>, <#Justification justification#>, <#bool onlyReduceInSize#>)
		_label.attachToComponent(&_slider, false);
	}
	
	
	Slider _slider;
	Label _label;
};
