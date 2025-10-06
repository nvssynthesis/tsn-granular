/*
  ==============================================================================

    TimbreSpaceComponent.h
    Created: 28 Oct 2023 4:24:15pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "fmt/core.h"
#include "../TsnGranularPluginProcessor.h"
#include "../../slicer_granular/Source/misc_util.h"
#include "../TimbreSpace/TimbreSpace.h"
/**
 TODO:
 -use legitimate 5D (or N-D) point class without mismatched smaller dimension subtypes. will need this to perform e.g. rotations
*/

struct ProgressIndicator	:	public juce::Component
{
	void paint(juce::Graphics &g) override;
	void resized() override;

	juce::String message {""};
	double progress {0.0};
};

class TimbreSpaceComponent	:	public juce::Component
, 								public juce::ChangeListener
, 								public juce::Thread::Listener
,								private juce::ActionListener
{
public:
	using Timbre2DPoint = nvs::timbrespace::Timbre2DPoint;
	using Timbre3DPoint = nvs::timbrespace::Timbre3DPoint;
	using TimbreSpace = nvs::timbrespace::TimbreSpace;

    explicit TimbreSpaceComponent(juce::AudioProcessor &proc);
	//==========================================================================================
	void changeListenerCallback (juce::ChangeBroadcaster* source) override;
	void actionListenerCallback (const juce::String &message) override;
	void exitSignalSent() override;
	//==========================================================================================
	void paint(juce::Graphics &g) override;
	void resized() override;
	void mouseDown (const juce::MouseEvent &event) override;
	void mouseUp (const juce::MouseEvent &event) override;
	void mouseDrag (const juce::MouseEvent &event) override;
	void mouseDragOrDown(juce::Point<int> mousePos);
	void mouseEnter(const juce::MouseEvent &event) override;
	void mouseExit (const juce::MouseEvent &event) override;
	void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
	//==========================================================================================

	void add5DPoint(Timbre2DPoint p2D, Timbre3DPoint p3D);
	void clear();
	std::vector<nvs::util::WeightedIdx> getCurrentPointIndices() const;
	
	void setNavigatorPoint(Timbre2DPoint p);
	ProgressIndicator& getProgressIndicator();
	

private:
	TSNGranularAudioProcessor *_proc {nullptr};
	
	ProgressIndicator progressIndicator;
	
	struct TSNMouse
	{
		void createMouseImage();
		TSNMouse() {
			createMouseImage();
		}
		Timbre3DPoint _uvz {0.f, 0.6f, 0.f};
		juce::Image _image;
		bool _dragging {false};
	};
	void updateCursor();
	
	TSNMouse tsn_mouse;
	
	struct NavigatorPoint {
		Timbre2DPoint _p2D {0.f, 0.f};
	} nav;
	
	void showAnalysisSaveDialog();
	class Callback 	:	public juce::ModalComponentManager::Callback
	{
	public:
		Callback(TimbreSpaceComponent &comp);
	private:
		void modalStateFinished(int choice) override;
		TimbreSpaceComponent &_ts_comp;
	};
	Callback *callback;

	void saveAnalysis();

	std::unique_ptr<juce::FileChooser> fileChooser;
	juce::Point<float> normalizePosition_neg1_pos1(juce::Point<int> pos);
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpaceComponent);
};
