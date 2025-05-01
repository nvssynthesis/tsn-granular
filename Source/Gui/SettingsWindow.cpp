/*
  ==============================================================================

    SettingsWindow.cpp
    Created: 10 Apr 2024 5:22:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "SettingsWindow.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"

SettingsWindow::SettingsWindow(TsnGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed, juce::Colour backgroundColour)
:	juce::DocumentWindow("Settings", backgroundColour, juce::DocumentWindow::allButtons)
{
	setAlwaysOnTop(true);
	constrainer.setMinimumSize(300, 300);
	setConstrainer(&constrainer);
	
	auto tabs = new juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop);
	tabs->addTab ("Onset",  juce::Colours::lightgrey, new OnsetSettingsComponent  (p, ed), true);
	tabs->addTab ("Timbre", juce::Colours::lightgrey, new TimbreSpaceSettingsComponent (p, ed), true);

	tabs->setSize (400, 300);
	setContentOwned (tabs, true);
}

void SettingsWindow::closeButtonPressed()  {
	delete this;
}
