/*
  ==============================================================================

    TimbreSpaceSettingsComponent.h
    Created: 1 May 2025 12:49:57am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#if false	// this is completely unused, opted for generic settings
#include <JuceHeader.h>
#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "../Analysis/Settings.h"
#include "NonAutomatableTitledSlider.h"
#include "../Analysis/Analyzer.h"

struct TimbreSpaceSettings
{
	nvs::analysis::Features feature_x ;
	nvs::analysis::Features feature_y ;
};

class TimbreSpaceSettingsComponent	:	public juce::Component,
										private juce::Button::Listener
{
public:
	TimbreSpaceSettingsComponent(TSNGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed);
	void resized() override;
private:
	TSNGranularAudioProcessor& proc;
	TsnGranularAudioProcessorEditor& editor;
	
	juce::ComboBox featureXMenu;
	juce::ComboBox featureYMenu;
	
	void handleXMenuChange();
	void handleYMenuChange();
	juce::TextButton applyButton;
	
	void buttonClicked(juce::Button *button) override;	// wanted to set settings in VT?
};

#endif
