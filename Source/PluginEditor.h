#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "params.h"
#include "dsp_util.h"
#include "Gui/FileSelectorComponent.h"
#include "Gui/WaveformComponent.h"
#include "Gui/SliderColumn.h"
#include "Gui/TimbreSpaceComponent.h"
#include "Gui/MainParamsComponent.h"

//==============================================================================
/** TODO:
	-slider below waveform view to select broad position
		-needs to be 'locking' to points eventually
*/

class TsaraGranularAudioProcessorEditor  : 	public juce::AudioProcessorEditor
,											public juce::FilenameComponentListener
,											public juce::ChangeListener
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
	void askForAnalysis();
	void paintMarkers(std::vector<float> onsetsInSeconds,
					  std::vector<std::vector<float>> PCA);
	
	void mouseDown(const juce::MouseEvent &event) override;
	void mouseDrag(const juce::MouseEvent &event) override;
private:
	juce::ComponentBoundsConstrainer constrainer;
	juce::Image backgroundImage;
	
	FileSelectorComponent fileComp;
	MainParamsComponent mainParamsComp;
	WaveformAndPositionComponent waveformAndPositionComponent;
	TimbreSpaceComponent timbreSpaceComponent;

	juce::ToggleButton triggeringButton;
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

	void update()
	{
		return;
	   
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
	
	TsaraGranularAudioProcessor& audioProcessor;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsaraGranularAudioProcessorEditor)
};
