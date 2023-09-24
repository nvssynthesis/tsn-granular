#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "params.h"
#include "dsp_util.h"
#include "FileSelectorComponent.h"
#include "WaveformComponent.h"
#include "SettingsWindow.h"
#include "SliderColumn.h"

//==============================================================================
/** TODO:
*/

struct MainParamsComponent	:	public juce::Component
{
	MainParamsComponent(TsaraGranularAudioProcessor& p)
	:
	attachedSliderColumnArray
	{
		SliderColumn(p.apvts, params_e::transpose),
		SliderColumn(p.apvts, params_e::position),
		SliderColumn(p.apvts, params_e::speed),
		SliderColumn(p.apvts, params_e::duration),
		SliderColumn(p.apvts, params_e::skew),
		SliderColumn(p.apvts, params_e::plateau),
		SliderColumn(p.apvts, params_e::pan)
	}
	{
		for (auto &s : attachedSliderColumnArray){
			addAndMakeVisible( s );
		}
	}
	void resized() override
	{
		auto localBounds = getLocalBounds();
		int const alottedCompHeight = localBounds.getHeight();// - y + smallPad;
		int const alottedCompWidth = localBounds.getWidth() / attachedSliderColumnArray.size();
		
		for (int i = 0; i < attachedSliderColumnArray.size(); ++i){
			int left = i * alottedCompWidth + localBounds.getX();
			attachedSliderColumnArray[i].setBounds(left, 0, alottedCompWidth, alottedCompHeight);
		}
	}
	
private:
	std::array<SliderColumn, static_cast<size_t>(params_e::count) / 2> attachedSliderColumnArray;
};

class TsaraGranularAudioProcessorEditor  : public juce::AudioProcessorEditor
//,			                                 public juce::Slider::Listener
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
	
	//===============================================================================
	void filenameComponentChanged (juce::FilenameComponent* fileComponentThatHasChanged) override;
	void readFile (const juce::File& fileToRead);
	//===============================================================================

private:
	juce::ComponentBoundsConstrainer constrainer;

	FileSelectorComponent fileComp;
//	std::array<SliderColumn, static_cast<size_t>(params_e::count) / 2> attachedSliderColumnArray;
	MainParamsComponent mainParamsComp;
	WaveformComponent waveformComponent;

	juce::ToggleButton triggeringButton;
	std::array<juce::Colour, 3> gradientColors {
		juce::Colours::darkgrey,
		juce::Colours::darkred,
//		juce::Colours::red,
//		juce::Colours::darkred,
		juce::Colours::darkgrey
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
