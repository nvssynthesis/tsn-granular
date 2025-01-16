//
//  OnsetSettingsComponent.h
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#pragma once
#include <JuceHeader.h>
#include "TsnGranularPluginEditor.h"
#include "../Analysis/Settings.h"
#include "NonAutomatableTitledSlider.h"
#include "fmt/core.h"

class OnsetSettingsComponent	:	public juce::Component,
									private juce::Button::Listener,
									private juce::Slider::Listener
{
public:
	OnsetSettingsComponent(juce::DocumentWindow &owner, TsnGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed);
	void paint(juce::Graphics &g) override;
	void placeMe(int const topPad, int const leftPad);
	void resized() override;
private:
	TsnGranularAudioProcessor& proc;
	TsnGranularAudioProcessorEditor& editor;
	nvs::analysis::analysisSettings _analysisSettings;
	nvs::analysis::onsetSettings _onsetSettings;
	
	NonAutomatableTitledSlider silenceThresholdSlider;
	juce::TextButton applyButton;
	juce::TextButton recalculateOnsetsButton;

	juce::DocumentWindow &_owner; // will actually be a SettingsWindow
	void sliderValueChanged (juce::Slider* slider) override;
	void buttonClicked(juce::Button *button) override;
};
