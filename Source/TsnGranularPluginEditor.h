#pragma once

#include "TsnGranularPluginProcessor.h"
#include "./Gui/TimbreSpaceComponent.h"
#include "./Navigation/LFO.h"
#include "../slicer_granular/Source/SlicerGranularPluginEditor.h"

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
	void paintOnsetMarkersAndTimbrePoints(std::vector<float> &onsetsInSeconds,
					  std::vector<std::vector<float>> &PCA);
	
	void mouseDown(const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
protected:
	void displayGrainDescriptions() override;
private:
	juce::ComponentBoundsConstrainer constrainer;
	juce::Image backgroundImage;
	
	nvs::nav::GUILFO &gui_lfo;
	TimbreSpaceComponent timbreSpaceComponent;

	std::array<juce::Colour, 3> gradientColors {
		juce::Colours::darkgrey,
		juce::Colours::darkred,
		juce::Colours::darkgrey
	};
	size_t colourOffsetIndex {0};
	
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

	void setPositionSliderFromChosenPoint();	// gets called by mouseDown, mouseDrag
		
	TsnGranularAudioProcessor& audioProcessor;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessorEditor)
};
