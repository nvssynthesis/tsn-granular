/*
  =============================================================================

    NavLFOPage.
    Created: 23 Jan 2025 4:16:56p
    Author:  Nicholas Sole

  =============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../slicer_granular/Source/Gui/AttachedSlider.h"

struct NavLFOParamsComponent	:	public juce::Component
{
	NavLFOParamsComponent(juce::AudioProcessorValueTreeState &apvts)
	:	sliderArray
	{
		AttachedSlider(apvts, params_e::nav_lfo_amount, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_rate, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_offset_x, juce::Slider::SliderStyle::LinearVertical),
		AttachedSlider(apvts, params_e::nav_lfo_offset_y, juce::Slider::SliderStyle::LinearVertical)
	}
	{
		for (auto &s : sliderArray){
			addAndMakeVisible(s._slider);
		}
	}
	void resized() override
	{
		auto localBounds = getLocalBounds();
		int const alottedCompHeight = localBounds.getHeight();// - y + smallPad;
		int const alottedCompWidth = localBounds.getWidth() / sliderArray.size();
		
		for (int i = 0; i < sliderArray.size(); ++i){
			int left = i * alottedCompWidth + localBounds.getX();
			sliderArray[i]._slider.setBounds(left, 0, alottedCompWidth, alottedCompHeight);
		}
	}
private:
	std::array<AttachedSlider,
			static_cast<size_t>(params_e::count_nav_lfo_params) - static_cast<size_t>(params_e::count_envelope_params) - 1
		> sliderArray;
};

struct NavLFOPage :	public juce::Component
{
	NavLFOPage(juce::AudioProcessorValueTreeState &apvts)
	:	navLfoParamsComp(apvts)
	{
		addAndMakeVisible(navLfoParamsComp);
	}
	void resized() override {
		navLfoParamsComp.setBounds(getLocalBounds());
	}
private:
	NavLFOParamsComponent navLfoParamsComp;
};

