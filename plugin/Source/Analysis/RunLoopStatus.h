/*
  ==============================================================================

    RunLoopStatus.h
    Created: 11 May 2025 10:41:58pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

namespace nvs::analysis {

// helpers needed for running long analyses with communication to other structures

class RunLoopStatus 	:	public juce::ChangeBroadcaster
{
public:
	void set(const juce::String &newMessage){
		message = newMessage;
		sendChangeMessage();
	}
	void set(const double newProgress){
		progress = newProgress;
		sendChangeMessage();
	}
	double getProgress() const {
		return progress.load();
	}
	juce::String getMessage() const {
		return message;
	}
private:
	juce::String message {""};	// ideally should have thread-safe alternative
	std::atomic<double> progress {0.0};
};
using ShouldExitFn = std::function<bool(void)>;

}
