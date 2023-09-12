/*
  ==============================================================================

    SettingsWindow.h
    Created: 12 Sep 2023 4:45:27am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

class SettingsWindow	:	public juce::DocumentWindow
{
public:
	SettingsWindow(juce::Colour backgroundColour)	:	juce::DocumentWindow("Settings", backgroundColour, juce::DocumentWindow::allButtons)
	, onsetSettingsComponent(*this)
	{
		setContentOwned(&onsetSettingsComponent, false);
		setContentComponentSize(200, 200);
	}
	~SettingsWindow(){
		std::cout << "destructing SettingsWindow\n";
	}
	void closeButtonPressed()  {
		delete this;
	}
	
	
private:
	class OnsetSettingsComponent	:	public juce::Component,
										private juce::Slider::Listener
	{
	public:
		OnsetSettingsComponent(SettingsWindow &owner)
		:	_owner(owner)
		{
			silenceThresholdSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
			silenceThresholdSlider.addListener(this);
			silenceThresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 30);
			silenceThresholdSlider.setNumDecimalPlacesToDisplay(2);
			silenceThresholdSlider.setRange(0.0, 1.0);
			addAndMakeVisible(&silenceThresholdSlider);
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
		void placeSlider(int const topPad, int const leftPad){
			int const sliderWidth = getWidth() / 10;
			int const sliderHeight = getHeight() - topPad*2;
			silenceThresholdSlider.setBounds(topPad, leftPad, sliderWidth, sliderHeight);
		}
		
		void resized() override{
			placeMe(10, 10);
			placeSlider(10, 10);
		}
	private:
		juce::Slider silenceThresholdSlider;
		SettingsWindow &_owner;
		void sliderValueChanged (juce::Slider* slider) override {
			if (slider == &silenceThresholdSlider){
				std::cout << "silence thresh: " << slider->getValue() << '\n';
			}
		}
	};
	OnsetSettingsComponent onsetSettingsComponent;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow);
};
