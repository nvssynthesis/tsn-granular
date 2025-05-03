//
//  OnsetSettingsComponent.cpp
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#include "OnsetSettingsComponent.h"

OnsetSettingsComponent::OnsetSettingsComponent(TsnGranularAudioProcessor& p, TsnGranularAudioProcessorEditor& ed)
:	proc(p),
	editor(ed),
	silenceThresholdSlider(juce::Slider::SliderStyle::LinearVertical, this, "silence threshold"),
	applyButton("Apply"),
	recalculateOnsetsButton("Recalculate Onsets")
{
	// update onsetSettings from those of the processor
	_onsetSettings = proc.getAnalyzer().getOnsetSettings();
	silenceThresholdSlider.setValue(_onsetSettings.silenceThreshold);

	addAndMakeVisible(&silenceThresholdSlider);
	
	applyButton.addListener(this);
	addAndMakeVisible(&applyButton);
	
	recalculateOnsetsButton.addListener(this);
	addAndMakeVisible(&recalculateOnsetsButton);
	
	setSize (100, 100);
}
void OnsetSettingsComponent::paint(juce::Graphics &g) {
	int const intensity = 140;
	g.setColour(juce::Colour(intensity, intensity, intensity));
	g.fillAll();
}

void OnsetSettingsComponent::resized() {
	int const pad = 10;
	int const sliderWidth = getWidth() / 10;
	int const sliderHeight = getHeight() - 2*pad;

	silenceThresholdSlider.setBounds(pad, pad, sliderWidth, sliderHeight);
	
	applyButton.setSize(100, 40);
	recalculateOnsetsButton.setSize(100, 40);
	int buttonY = getHeight() - 10 - 60 - 10 - 10;
	buttonY += 40;
	const int buttonYcentre = buttonY + (applyButton.getHeight() / 2);
	applyButton.			setCentrePosition	((getWidth() / 3) * 1, buttonYcentre);
	recalculateOnsetsButton.setCentrePosition	((getWidth() / 3) * 2, buttonYcentre);
}

void OnsetSettingsComponent::sliderValueChanged (juce::Slider* slider) {
	if (slider == silenceThresholdSlider.getSlider()){
		_onsetSettings.silenceThreshold = slider->getValue();
	}
}
void OnsetSettingsComponent::buttonClicked(juce::Button *button) {
	if (button == &applyButton){
		proc.getAnalyzer().setOnsetSettings(_onsetSettings);
	}
	else if (button == &recalculateOnsetsButton){
		proc.getAnalyzer().setOnsetSettings(_onsetSettings);
		proc.askForAnalysis();
	}
}
