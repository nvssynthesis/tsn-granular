/*
  ==============================================================================

    SettingsWindow.h
    Created: 12 Sep 2023 4:45:27am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "./PluginEditor.h"
#include "Gui/OnsetSettingsComponent.h"

class SettingsWindow	:	public juce::DocumentWindow
{
public:
	SettingsWindow(TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed, juce::Colour backgroundColour);
	void closeButtonPressed();
	
private:
	juce::ComponentBoundsConstrainer constrainer;
	OnsetSettingsComponent onsetSettingsComponent;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow);
};
