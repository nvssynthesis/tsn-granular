//
//  OnsetSettingsComponent.h
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#ifndef OnsetSettingsComponent_h
#define OnsetSettingsComponent_h

#include <JuceHeader.h>
#include "../PluginEditor.h"	// without explicating the parent directory, preprocessor was finding PluginEditor in slicer_granular
#include "../Analysis/Settings.h"

//class SettingsWindow;
//class TsaraGranularAudioProcessor;
//class TsaraGranularAudioProcessorEditor;

class OnsetSettingsComponent	:	public juce::Component,
									private juce::Button::Listener,
									private juce::Slider::Listener
{
public:
	OnsetSettingsComponent(juce::DocumentWindow &owner, TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed);
	void paint(juce::Graphics &g) override;
	void placeMe(int const topPad, int const leftPad);
	void placeSlider(int const topPad, int const leftPad, int const bottomPad);
	void resized() override;
private:
	TsaraGranularAudioProcessor& proc;
	TsaraGranularAudioProcessorEditor& editor;
	nvs::analysis::analysisSettings _analysisSettings;
	nvs::analysis::onsetSettings _onsetSettings;
	
	juce::Slider silenceThresholdSlider;
	juce::TextButton applyButton;
	juce::TextButton recalculateOnsetsButton;

	juce::DocumentWindow &_owner; // will actually be a SettingsWindow
	void sliderValueChanged (juce::Slider* slider) override;
	void buttonClicked(juce::Button *button) override;
};

#endif /* OnsetSettingsComponent_h */
