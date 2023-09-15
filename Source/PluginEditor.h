#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "params.h"
#include "dsp_util.h"
#include "FileSelectorComponent.h"
#include "SettingsWindow.h"

//==============================================================================
/** TODO:
	-center param names
	 -add button for onset calculate
	 -add button for write events to wav
 
 Instead of manually adding sliders, labels, and knobs in separate rows, make a component that contains all of them properly aligned.
*/

struct AttachedSlider {
	using Slider = juce::Slider;
	using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
	
	AttachedSlider(juce::AudioProcessorValueTreeState &apvts, params_e param, Slider::SliderStyle sliderStyle)	:
	_slider(),
	_attachment(apvts, getParamName(param), _slider)
	{
		_slider.setSliderStyle(sliderStyle);
		_slider.setNormalisableRange(getNormalizableRange<double>(param));
		_slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 25);
		_slider.setValue(getParamDefault(param));

		_slider.setColour(juce::Slider::ColourIds::thumbColourId, juce::Colours::red);
		_slider.setColour(juce::Slider::ColourIds::textBoxTextColourId, juce::Colours::black);
	}
	
	Slider _slider;
	SliderAttachment _attachment;
};

/*
 embeds a linear vertical slider, label, and knob into a single component
 */
class SliderColumn	:	public juce::Component
{
public:
	using Slider = juce::Slider;
	using SliderAttachment = juce::SliderParameterAttachment;
	
	SliderColumn(juce::AudioProcessorValueTreeState &apvts, params_e nonRandomParam)
	:
	_slider(apvts, nonRandomParam, juce::Slider::LinearVertical),
	_knob(apvts, mainToRandom(nonRandomParam), juce::Slider::RotaryHorizontalVerticalDrag)
	{
		addAndMakeVisible(&_slider._slider);
		
		_label.setText(getParamName(nonRandomParam), juce::dontSendNotification);
		
//		_label.setFont( juce::Font("Copperplate", "Regular", proportionOfWidth(0.5f)) );
		_label.setJustificationType(juce::Justification::centred);
		addAndMakeVisible(&_label);
		
		_knob._slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
		addAndMakeVisible(&_knob._slider);
	}
	
	void paint(juce::Graphics& g) override {
		g.setColour((juce::Colour(juce::Colours::whitesmoke)).withAlpha(0.5f));
		auto localBounds = getLocalBounds().toFloat();
		localBounds.reduce(0.93f, 0.93f);
		g.fillEllipse(localBounds);
		
		_label.setFont( juce::Font("Copperplate", "Regular", proportionOfWidth(0.14f)) );
	}
	void resized() override {
		float sliderProportion {0.75f};
		float labelProportion {0.1f};
		float knobProportion {0.15f};
		float proportionSum = sliderProportion + labelProportion + knobProportion;
		// would compare with 1 but floating point imprecision
		jassert (proportionSum > 0.999f);
		jassert (proportionSum < 1.001f);
		
		juce::Rectangle<int> localBounds = getLocalBounds();
		localBounds.reduce(5, 5);
		
		int const sliderToLabelPadding = 10;
		int const labelToKnobPadding = 15;
		
		int x(localBounds.getX()), y(localBounds.getY());

		int const sliderHeight = localBounds.getHeight() * sliderProportion - sliderToLabelPadding;
		_slider._slider.setBounds(x, y, localBounds.getWidth(), sliderHeight);
		y += sliderHeight;
		
		y += sliderToLabelPadding;
		
		int const labelHeight = localBounds.getHeight() * labelProportion;// - labelToKnobPadding;
		_label.setBounds(x, y, localBounds.getWidth(), labelHeight);
		y += labelHeight;
		
//		y += labelToKnobPadding;
		
		int const knobHeight = localBounds.getHeight() * knobProportion;
		_knob._slider.setBounds(x, y, localBounds.getWidth(), knobHeight);
	}
private:
	AttachedSlider _slider;
	juce::Label _label;
	AttachedSlider _knob;
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
#if 0
	std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, static_cast<size_t>(params_e::count)> paramSliderAttachments;
#endif
private:
#if 0
	std::array<juce::Slider, static_cast<size_t>(params_e::count)> paramSliders;
	std::array<juce::Label, static_cast<size_t>(params_e::count)> paramLabels;
#endif
	juce::ComponentBoundsConstrainer constrainer;

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

	// must be initialized after audioProcessor
	std::array<SliderColumn, static_cast<size_t>(params_e::count) / 2> attachedSliderColumnArray;

	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsaraGranularAudioProcessorEditor)
};
