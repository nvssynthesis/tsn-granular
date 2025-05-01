//
//  OnsetSettingsComponent.h
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#pragma once
#include <JuceHeader.h>
#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "../Analysis/Settings.h"
#include "NonAutomatableTitledSlider.h"

class OnsetSettingsComponent	:	public juce::Component,
									private juce::Button::Listener,
									private juce::Slider::Listener
{
public:
	OnsetSettingsComponent(TsnGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed);
	void paint(juce::Graphics &g) override;
	void placeMe(int const topPad, int const leftPad);
	void resized() override;
private:
	TsnGranularAudioProcessor& proc;
	TsnGranularAudioProcessorEditor& editor;
	nvs::analysis::analysisSettings _analysisSettings;
	nvs::analysis::onsetSettings _onsetSettings;
	
	NonAutomatableTitledSlider silenceThresholdSlider;
	
	// need some sort of checkmark buttons in a list for features to include
	
	// need some sort of mutually exclusive checkmark buttons in a list for dimensionality reduction type to use
	
	juce::TextButton applyButton;
	juce::TextButton recalculateOnsetsButton;

	void sliderValueChanged (juce::Slider* slider) override;
	void buttonClicked(juce::Button *button) override;
};
