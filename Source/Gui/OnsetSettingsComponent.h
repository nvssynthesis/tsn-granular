//
//  OnsetSettingsComponent.h
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#pragma once
#include <JuceHeader.h>
#include "../PluginEditor.h"// without explicating parent, preprocessor was finding PluginEditor in slicer_granular
#include "../Analysis/Settings.h"
#include "NonAutomatableTitledSlider.h"
#include "fmt/core.h"

class OnsetSettingsComponent	:	public juce::Component,
									private juce::Button::Listener,
									private juce::Slider::Listener
{
public:
	OnsetSettingsComponent(juce::DocumentWindow &owner, TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed);
	void paint(juce::Graphics &g) override;
	void placeMe(int const topPad, int const leftPad);
	void resized() override;
private:
	TsaraGranularAudioProcessor& proc;
	TsaraGranularAudioProcessorEditor& editor;
	nvs::analysis::analysisSettings _analysisSettings;
	nvs::analysis::onsetSettings _onsetSettings;
	
	NonAutomatableTitledSlider silenceThresholdSlider;
	juce::TextButton applyButton;
	juce::TextButton recalculateOnsetsButton;

	juce::DocumentWindow &_owner; // will actually be a SettingsWindow
	void sliderValueChanged (juce::Slider* slider) override;
	void buttonClicked(juce::Button *button) override;
};
