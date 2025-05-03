/*
  ==============================================================================

    SettingsWindow.h
    Created: 12 Sep 2023 4:45:27am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "./TsnGranularPluginEditor.h"
#include "Gui/OnsetSettingsComponent.h"
#include "Gui/TimbreSpaceSettingsComponent.h"
#include "../Analysis/Settings.h"

class SettingsWindow  : public juce::DocumentWindow
{
public:
	SettingsWindow (TsnGranularAudioProcessor& processor, juce::Colour backgroundColour);

	void closeButtonPressed() override;

private:
	TsnGranularAudioProcessor& proc;
	std::unique_ptr<juce::TabbedComponent> tabs;
	juce::ComponentBoundsConstrainer constrainer;

	// Create a component containing all controls for one branch
	juce::Component* createPageForBranch (juce::ValueTree& settingsVT, const juce::String& branchName, const std::map<juce::String,nvs::analysis::AnySpec>& specMap);
};
