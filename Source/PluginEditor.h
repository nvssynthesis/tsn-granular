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
