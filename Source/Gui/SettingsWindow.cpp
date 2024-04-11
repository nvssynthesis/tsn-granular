/*
  ==============================================================================

    SettingsWindow.cpp
    Created: 10 Apr 2024 5:22:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "SettingsWindow.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"

SettingsWindow::SettingsWindow(TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed, juce::Colour backgroundColour)
:	juce::DocumentWindow("Settings", backgroundColour, juce::DocumentWindow::allButtons)
, 	onsetSettingsComponent(*this, p, ed)
{
	setAlwaysOnTop(true);
	constrainer.setMinimumSize(300, 300);
	setContentOwned(&onsetSettingsComponent, false);
	setConstrainer(&constrainer);
}

void SettingsWindow::closeButtonPressed()  {
	delete this;
}
