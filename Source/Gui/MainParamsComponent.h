/*
  ==============================================================================

    MainParamsComponent.h
    Created: 5 Dec 2023 12:39:27pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

struct MainParamsComponent	:	public juce::Component
{
	MainParamsComponent(TsaraGranularAudioProcessor& p)
	:
	attachedSliderColumnArray
	{
		SliderColumn(p.apvts, params_e::transpose),
		SliderColumn(p.apvts, params_e::position),
		SliderColumn(p.apvts, params_e::speed),
		SliderColumn(p.apvts, params_e::duration),
		SliderColumn(p.apvts, params_e::skew),
		SliderColumn(p.apvts, params_e::plateau),
		SliderColumn(p.apvts, params_e::pan)
	}
	{
		for (auto &s : attachedSliderColumnArray){
			addAndMakeVisible( s );
		}
	}
	void resized() override
	{
		auto localBounds = getLocalBounds();
		int const alottedCompHeight = localBounds.getHeight();// - y + smallPad;
		int const alottedCompWidth = localBounds.getWidth() / attachedSliderColumnArray.size();
		
		for (int i = 0; i < attachedSliderColumnArray.size(); ++i){
			int left = i * alottedCompWidth + localBounds.getX();
			attachedSliderColumnArray[i].setBounds(left, 0, alottedCompWidth, alottedCompHeight);
		}
	}
	void setSliderParam(params_e param, double val){
		assert(static_cast<size_t>(param) < (static_cast<size_t>(params_e::count) / 2) );
		attachedSliderColumnArray[static_cast<size_t>(param)].setVal(val);
	}
private:
	std::array<SliderColumn, static_cast<size_t>(params_e::count) / 2> attachedSliderColumnArray;
};
