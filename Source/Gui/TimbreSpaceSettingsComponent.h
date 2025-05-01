/*
  ==============================================================================

    TimbreSpaceSettingsComponent.h
    Created: 1 May 2025 12:49:57am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
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
	TimbreSpaceSettingsComponent(TsnGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed)
	:	proc(p), editor(ed)
	{
		using namespace nvs::analysis;
		
		for (auto f : featuresIterator()){
			featureXMenu.addItem(toString(f), int(f) + 1);
			featureYMenu.addItem(toString(f), int(f) + 1);
		}
		featureXMenu.onChange = [this]() {
			handleXMenuChange();
		};
		featureYMenu.onChange = [this]() {
			handleYMenuChange();
		};
		addAndMakeVisible(featureXMenu);
		addAndMakeVisible(featureYMenu);
		
		setSize (100, 100);
	}
	void paint(juce::Graphics &g) override {}
	void resized() override {
		featureXMenu.setBounds(10, 10, 200, 24);
		featureYMenu.setBounds(10, 36, 200, 24);
	}
private:
	TsnGranularAudioProcessor& proc;
	TsnGranularAudioProcessorEditor& editor;
	
	juce::ComboBox featureXMenu;
	juce::ComboBox featureYMenu;
	
	void handleXMenuChange()
	{
		using namespace nvs::analysis;
		int selectedId = featureXMenu.getSelectedId();
		std::cout << "x menu selection: " << toString(Features(selectedId)) << " \n";
	}
	void handleYMenuChange()
	{
		using namespace nvs::analysis;
		int selectedId = featureYMenu.getSelectedId();
		std::cout << "y menu selection: " << toString(Features(selectedId)) << " \n";
	}
	juce::TextButton applyButton;
	
	void buttonClicked(juce::Button *button) override {
		if (button == &applyButton){
			// set settings in value tree!
		}
	}
};

