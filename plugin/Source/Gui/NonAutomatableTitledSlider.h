/*
  ==============================================================================

    NonAutomatableTitledSlider.h
    Created: 10 Apr 2024 5:44:02pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "fmt/core.h"

struct NonAutomatableTitledSlider : public juce::Component
{
	using Slider = juce::Slider;
	using Label = juce::Label;
	
	NonAutomatableTitledSlider(Slider::SliderStyle sliderStyle,
							juce::Slider::Listener *listener,
							   juce::String name,
				   juce::Slider::TextEntryBoxPosition entryPos = juce::Slider::TextBoxBelow)
	:
	_name(name.replaceCharacter(' ', '\n')),
	_slider(),
	_label(_name, _name)
	{
		addChildComponent(_slider);
		addChildComponent(_label);
		
		constrainer.setMinimumWidth(80);
		
		_slider.setSliderStyle(sliderStyle);
		_slider.setTextBoxStyle(entryPos, false, 50, 25);
		_slider.setNumDecimalPlacesToDisplay(2);
		
		_slider.setColour(Slider::ColourIds::thumbColourId, juce::Colours::palevioletred);
		_slider.setColour(Slider::ColourIds::textBoxTextColourId, juce::Colours::lightgrey);
		_slider.setColour(Slider::ColourIds::backgroundColourId, juce::Colours::white);
		
		_slider.setName(name);
		_slider.setTitle(name);
		
		_slider.setRange(0.0, 1.0);

		_slider.addListener(listener);
		addAndMakeVisible(_slider);
		addAndMakeVisible(_label);
	}
	void paint(juce::Graphics &) override {}
	void resized() override {
		fmt::print("non automatable slider {} resized\n", _name.toStdString());
		constrainer.checkComponentBounds(this);

		int const topPad = 10;
		int const leftPad = 10;
		int const bottomPad = 60;
		auto const bounds = getBounds();
		int const label_h = 40;//_label.getHeight();
		auto const labelBounds = bounds.withHeight(label_h);
		_label.setBounds(labelBounds);
		
		int const slider_h = getHeight() - label_h - bottomPad;
		_slider.setBounds(leftPad, topPad + label_h, getWidth(), slider_h);
	}
	void setValue(double value){
		_slider.setValue(value);
	}
	Slider const *getSlider() const {
		return &_slider;
	}
private:
	juce::String _name;
	Slider _slider;
	Label _label;
	juce::ComponentBoundsConstrainer constrainer;
};

