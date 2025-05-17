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
,											public juce::ValueTree::Listener	// to listen for timbre space settings change, to redraw points
,											public GranularEditorCommon
{
public:
	TsnGranularAudioProcessorEditor (TsnGranularAudioProcessor&);
	~TsnGranularAudioProcessorEditor() override;
	//==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;
	//===============================================================================
	void paintOnsetMarkersAndTimbrePoints(std::vector<float> const &onsets);
	
	void mouseDown(const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
	void valueTreePropertyChanged (juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override;
	ProgressIndicator &getProgressIndicator() {
		return timbreSpaceComponent.getProgressIndicator();
	}
protected:
private:
	juce::ComponentBoundsConstrainer constrainer;
	
	void updateAndDrawTimbreSpacePoints(bool verbose = false);
	void drawTimbreSpacePoints(bool verbose = false);
	struct TimbreSpaceDrawingSettings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Features> dimensionWisefeatures {
			nvs::analysis::Features::bfcc1,
			nvs::analysis::Features::bfcc2,
			nvs::analysis::Features::bfcc3,
			nvs::analysis::Features::bfcc4,
			nvs::analysis::Features::bfcc5
		};
	};
	TimbreSpaceDrawingSettings timbreSpaceDrawingSettings;
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

	void setReadBoundsFromChosenPoint();	// gets called by mouseDown, mouseDrag
		
	TsnGranularAudioProcessor& audioProcessor;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessorEditor)
};
