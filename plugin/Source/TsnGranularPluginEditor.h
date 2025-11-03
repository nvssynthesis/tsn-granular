#pragma once

#include "TsnGranularPluginProcessor.h"
#include "./Gui/TimbreSpaceComponent.h"
#include "../slicer_granular/Source/SlicerGranularPluginEditor.h"
#include "Gui/LAF.h"

//==============================================================================

class TsnGranularAudioProcessorEditor final : 	public juce::AudioProcessorEditor
,											    public GranularEditorCommon
,                                               private juce::Timer

{
public:
	TsnGranularAudioProcessorEditor (TSNGranularAudioProcessor&);
	~TsnGranularAudioProcessorEditor() override;
	//==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;
	//===============================================================================
	void mouseDown(const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
	//===============================================================================
    void timerCallback() override;
	//===============================================================================
	ProgressIndicator &getProgressIndicator() {
		return timbreSpaceComponent.getProgressIndicator();
	}
private:
	nvs::gui::LAF laf;
	TimbreSpaceComponent timbreSpaceComponent;
	//===============================================================================
	void drawBackground();
	//===============================================================================
	juce::Image  backgroundImage;
	bool backgroundNeedsUpdate;
	std::array<juce::Colour, 3> gradientColors {
		juce::Colours::darkgrey,
		juce::Colours::transparentBlack,
		juce::Colours::darkgrey
	};
	size_t colourOffsetIndex {0};
	//=================================================================
	juce::TextButton askForAnalysisButton;
	juce::TextButton writeWavsButton;
	juce::TextButton settingsButton;
	//=================================================================
	void popupSettings(bool native);
	juce::Array<juce::Component::SafePointer<juce::Component>> windows;
	void closeAllWindows();
	//=================================================================
	TSNGranularAudioProcessor& TSNaudioProcessor;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessorEditor)
};
