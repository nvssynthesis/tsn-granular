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

class SettingsWindow final : public DocumentWindow
{
public:
	SettingsWindow (TSNGranularAudioProcessor& processor, Colour backgroundColour);

	void closeButtonPressed() override;

private:
	TSNGranularAudioProcessor& proc;
	std::unique_ptr<TabbedComponent> tabs;
	ComponentBoundsConstrainer constrainer;

    TooltipWindow tooltipWindow {this};

	// Create a component containing all controls for one branch
	static Component* createPageForBranch (ValueTree& settingsVT, const String& branchName,
	    const std::map<String, nvs::analysis::modern::AnySpec>& specMap);
};
