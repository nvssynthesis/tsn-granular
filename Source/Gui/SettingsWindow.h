/*
  ==============================================================================

    SettingsWindow.h
    Created: 12 Sep 2023 4:45:27am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "PluginEditor.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"

class SettingsWindow	:	public juce::DocumentWindow
{
	juce::ComponentBoundsConstrainer constrainer;
public:
	SettingsWindow(TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed, juce::Colour backgroundColour)
	:	juce::DocumentWindow("Settings", backgroundColour, juce::DocumentWindow::allButtons)
	, 	onsetSettingsComponent(*this, p, ed)
	{
		setAlwaysOnTop(true);
		constrainer.setMinimumSize(300, 300);
		setContentOwned(&onsetSettingsComponent, false);
		setConstrainer(&constrainer);
	}
	~SettingsWindow(){
		std::cout << "destructing SettingsWindow\n";
	}
	void closeButtonPressed()  {
		delete this;
	}
	
private:
	class OnsetSettingsComponent	:	public juce::Component,
										private juce::Button::Listener,
										private juce::Slider::Listener
	{
	public:
		OnsetSettingsComponent(SettingsWindow &owner, TsaraGranularAudioProcessor& p, TsaraGranularAudioProcessorEditor& ed)
		:	proc(p),
			editor(ed),
			applyButton("Apply"),
			recalculateOnsetsButton("Recalculate Onsets"),
			_owner(owner)
		{
			silenceThresholdSlider.addListener(this);
			silenceThresholdSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
			silenceThresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 30);
			silenceThresholdSlider.setNumDecimalPlacesToDisplay(2);
			silenceThresholdSlider.setRange(0.0, 1.0);
			addAndMakeVisible(&silenceThresholdSlider);
			
			applyButton.addListener(this);
			addAndMakeVisible(&applyButton);
			
			recalculateOnsetsButton.addListener(this);
			addAndMakeVisible(&recalculateOnsetsButton);
			
			setSize (100, 100);
		}
		void paint(juce::Graphics &g) override{
			int const intensity = 140;
			g.setColour(juce::Colour(intensity, intensity, intensity));
			g.fillAll();
		}
		void placeMe(int const topPad, int const leftPad){
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
		void placeSlider(int const topPad, int const leftPad, int const bottomPad){
			int const sliderWidth = getWidth() / 10;
			int const sliderHeight = getHeight() - topPad - bottomPad;
			silenceThresholdSlider.setBounds(topPad, leftPad, sliderWidth, sliderHeight);
		}
		void resized() override{
			placeMe(10, 10);			// top now +10 +10
			placeSlider(10, 10, 60);	// top now +10 +60
			
			applyButton.setSize(100, 40);
			recalculateOnsetsButton.setSize(100, 40);
			int buttonY = getHeight() - 10 - 60 - 10 - 10;
			buttonY += 40;
			const int buttonYcentre = buttonY + (applyButton.getHeight() / 2);
			applyButton.			setCentrePosition	((getWidth() / 3) * 1, buttonYcentre);
			recalculateOnsetsButton.setCentrePosition	((getWidth() / 3) * 2, buttonYcentre);
		}
	private:
		TsaraGranularAudioProcessor& proc;
		TsaraGranularAudioProcessorEditor& editor;
		nvs::analysis::analysisSettings _analysisSettings;
		nvs::analysis::onsetSettings _onsetSettings;
		
		juce::Slider silenceThresholdSlider;
		juce::TextButton applyButton;
		juce::TextButton recalculateOnsetsButton;

		SettingsWindow &_owner;
		void sliderValueChanged (juce::Slider* slider) override {
			if (slider == &silenceThresholdSlider){
				_onsetSettings.silenceThreshold = slider->getValue();
			}
		}
		void buttonClicked(juce::Button *button) override {
			if (button == &applyButton){
				proc.setOnsetSettings(_onsetSettings);
			}
			else if (button == &recalculateOnsetsButton){
				proc.setOnsetSettings(_onsetSettings);
				editor.askForAnalysis();
			}
		}
	};
	OnsetSettingsComponent onsetSettingsComponent;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow);
};
