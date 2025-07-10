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

class TimbreSpaceComponent	:	public juce::Component, public juce::ChangeListener, public juce::Thread::Listener
{
public:
	using timbre2DPoint = nvs::timbrespace::timbre2DPoint;
	using timbre3DPoint = nvs::timbrespace::timbre3DPoint;
	using TimbreSpace = nvs::timbrespace::TimbreSpace;
	
	struct Navigator {
		timbre2DPoint _p2D {0.f, 0.f};
	};

	TimbreSpaceComponent(juce::AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace);
	
	void changeListenerCallback (juce::ChangeBroadcaster* source) override;
	void exitSignalSent() override;
	
	void add5DPoint(timbre2DPoint p2D, timbre3DPoint p3D);
	void clear();
	void paint(juce::Graphics &g) override;
	void resized() override;
	
	void mouseDown (const juce::MouseEvent &event) override;
	void mouseUp (const juce::MouseEvent &event) override;
	void mouseDrag (const juce::MouseEvent &event) override;
	void mouseDragOrDown(juce::Point<int> mousePos);
	void mouseEnter(const juce::MouseEvent &event) override;
	void mouseExit (const juce::MouseEvent &event) override;
	void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
	
	std::vector<nvs::util::WeightedIdx> getCurrentPointIndices() const;
	
	void setNavigatorPoint(timbre2DPoint p);
	ProgressIndicator& getProgressIndicator();
private:
	juce::AudioProcessorValueTreeState &_apvts;
	
	ProgressIndicator progressIndicator;
	
	struct TSNMouse
	{
		void createMouseImage();
		TSNMouse() {
			createMouseImage();
			fmt::print("tsn mouse constructed\n");
		}
		timbre3DPoint _uvz {0.f, 0.6f, 0.f};
		juce::Image _image;
		bool _dragging {false};
	};
	void updateCursor();
	
	TSNMouse tsn_mouse;
	Navigator nav;
	
	TimbreSpace &_timbreSpace;
	
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
