#pragma once

#include "TsnGranularPluginProcessor.h"
#include "./Gui/TimbreSpaceComponent.h"
#include "./Navigation/LFO.h"
#include "../slicer_granular/Source/SlicerGranularPluginEditor.h"
#include "Gui/LAF.h"

//==============================================================================
/** TODO:
	-slider below waveform view to select broad position
		-needs to be 'locking' to points eventually
*/

class TsnGranularAudioProcessorEditor  : 	public juce::AudioProcessorEditor
,											public GranularEditorCommon
{
public:
	TsnGranularAudioProcessorEditor (TsnGranularAudioProcessor&);
	~TsnGranularAudioProcessorEditor() override;
	//==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;
	//===============================================================================
	void paintOnsetMarkers();
	
	void mouseDown(const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
	ProgressIndicator &getProgressIndicator() {
		return timbreSpaceComponent.getProgressIndicator();
	}
protected:
private:
	nvs::gui::LAF laf;

	TimbreSpaceComponent timbreSpaceComponent;

	std::array<juce::Colour, 3> gradientColors {
		juce::Colours::darkgrey,
		juce::Colours::transparentBlack,
		juce::Colours::darkgrey
	};
	size_t colourOffsetIndex {0};
	
	void drawBackground();

	juce::Image  backgroundImage;
	bool backgroundNeedsUpdate;
	
	juce::TextButton askForAnalysisButton;
	juce::TextButton writeWavsButton;
	juce::TextButton settingsButton;
	juce::ComboBox positionQuantizeStrengthComboBox;
	//=================================================================
	void popupSettings(bool native);
	juce::Array<juce::Component::SafePointer<juce::Component>> windows;
	//=================================================================
	void closeAllWindows();
	//=================================================================

	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
		
	TsnGranularAudioProcessor& audioProcessor;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessorEditor)
};
