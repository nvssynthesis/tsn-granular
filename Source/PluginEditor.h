#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "params.h"
#include "dsp_util.h"
#include "FileSelectorComponent.h"

//==============================================================================
/** TODO:
	-center param names
	 -add button for onset calculate
	 -add button for write events to wav
*/
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
	class OnsetSettingsComponent	:	public juce::Component
	{
	public:
		OnsetSettingsComponent(SettingsWindow &owner)
		:	_owner(owner)
		{
			addAndMakeVisible(&silenceThresholdSlider);
			setSize (100, 100);
		}
		void paint(juce::Graphics &g) override{
			g.setColour(juce::Colour(20, 200, 200));
			g.fillAll();
		}
		void resized() override{
#if 0	// for some reason this does not work as expected
			auto *parent = dynamic_cast<SettingsWindow *>(  getParentComponent() );
			if (parent){
				std::cout << "SettingsWindow dynamic cast success\n";
				juce::Rectangle<float> dimen = getBoundsInParent().toFloat();
				dimen.setHeight( dimen.getHeight() - parent->getTitleBarHeight() ) ;
				dimen *= 0.5f;
				setBounds(dimen.toNearestInt());
				return;
			}
#endif
			juce::Rectangle<int> bounds = _owner.getBounds();
			int const barPad = 30;
			int const bottomPad = 10;
			bounds.setHeight(bounds.getHeight() - barPad - bottomPad);
			bounds.setX(10);
			bounds.setWidth(bounds.getWidth() - 10*2);
			bounds.setY(barPad);
			std::cout << "x: " << bounds.getX()
						<< ", y: " << bounds.getY()
						<< ", w: " << bounds.getWidth()
						<< ", h: " << bounds.getHeight() << '\n';
			setBounds(bounds);
		}
	private:
		juce::Slider silenceThresholdSlider;
		SettingsWindow &_owner;
	};
	OnsetSettingsComponent onsetSettingsComponent;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow);
};

class TsaraGranularAudioProcessorEditor  : public juce::AudioProcessorEditor
,			                                 public juce::Slider::Listener
,											 public juce::FilenameComponentListener
{
public:
	TsaraGranularAudioProcessorEditor (TsaraGranularAudioProcessor&);
	~TsaraGranularAudioProcessorEditor() override;
	//==============================================================================
	void updateToggleState (juce::Button* button, juce::String name, bool &valToAffect);
	//==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;
	
	void sliderValueChanged(juce::Slider*) override;
	
	//===============================================================================
	void filenameComponentChanged (juce::FilenameComponent* fileComponentThatHasChanged) override;
	void readFile (const juce::File& fileToRead);
	//===============================================================================

	std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, static_cast<size_t>(params_e::count)> paramSliderAttachments;

private:
	std::array<juce::Slider, static_cast<size_t>(params_e::count)> paramSliders;
	std::array<juce::Label, static_cast<size_t>(params_e::count)> paramLabels;
	FileSelectorComponent fileComp;

	juce::ToggleButton triggeringButton;
	std::array<juce::Colour, 5> gradientColors {
		juce::Colours::transparentBlack,
		juce::Colours::darkred,
		juce::Colours::red,
		juce::Colours::darkred,
		juce::Colours::black
	};
	size_t colourOffsetIndex {0};
	
	juce::TextButton calculateOnsetsButton;
	juce::TextButton writeWavsButton;
	juce::TextButton settingsButton;
	//=================================================================
	void popupSettings(bool native);
	juce::Array<juce::Component::SafePointer<juce::Component>> windows;
	//=================================================================
	void closeAllWindows();
	//=================================================================

	void update()
	{
		return;
//		const auto needsToRepaint = updateState();
	   
		const float level = audioProcessor.rmsInformant.val;
		const float recentLevel = audioProcessor.rmsWAinformant.val;

		const bool needsToRepaint = (level > (recentLevel * 1.2f));
		
		if (needsToRepaint){
			++colourOffsetIndex;
			colourOffsetIndex %= gradientColors.size();
			repaint();
		}
	}
	juce::VBlankAttachment vbAttachment { this, [this] { update(); } };
	
	// button to recompute analysis
	
	// sliders to change analysis settings
	
	TsaraGranularAudioProcessor& audioProcessor;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsaraGranularAudioProcessorEditor)
};
