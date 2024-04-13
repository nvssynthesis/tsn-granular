//
//  OnsetSettingsComponent.cpp
//  tsara-granular
//
//  Created by Nicholas Solem on 4/10/24.
//  Copyright © 2024 nvssynthesis. All rights reserved.
//

#include "OnsetSettingsComponent.h"

OnsetSettingsComponent::OnsetSettingsComponent(juce::DocumentWindow &owner, TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed)
:	proc(p),
	editor(ed),
	silenceThresholdSlider(juce::Slider::SliderStyle::LinearVertical, this, "silence\nthreshold"),
	applyButton("Apply"),
	recalculateOnsetsButton("Recalculate Onsets"),
	_owner(owner)
{
	silenceThresholdSlider._slider.setRange(0.0, 1.0);
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
void OnsetSettingsComponent::placeMe(int const topPad, int const leftPad){
	juce::Rectangle<int> bounds = _owner.getBounds();
	
	int const barPad = _owner.getTitleBarHeight();
	int const compHeight = bounds.getHeight() - barPad - topPad*2;
	int const compWidth = bounds.getWidth() - leftPad*2;
	bounds.setHeight(compHeight);
	bounds.setX(leftPad);
	bounds.setWidth(compWidth);
	bounds.setY(barPad + topPad);
	setBounds(bounds);
}

void OnsetSettingsComponent::resized() {
	placeMe(10, 10);			// top now +10 +10
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
	if (slider == &silenceThresholdSlider._slider){
		_onsetSettings.silenceThreshold = slider->getValue();
	}
}
void OnsetSettingsComponent::buttonClicked(juce::Button *button) {
	if (button == &applyButton){
		proc.setOnsetSettings(_onsetSettings);
	}
	else if (button == &recalculateOnsetsButton){
		proc.setOnsetSettings(_onsetSettings);
		editor.askForAnalysis();
	}
}
