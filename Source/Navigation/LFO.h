/*
  ==============================================================================

    LFO.h
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <functional>

namespace nvs::nav {

class GUILFO : public juce::Timer
{
public:
	GUILFO(juce::AudioProcessorValueTreeState &apvts, double updateRateHz);
	~GUILFO() override = default;
	void start();
	void stop();
	void setOnUpdateCallback(std::function<void(double, double)> callback);

protected:
	void timerCallback() override;

private:
	juce::AudioProcessorValueTreeState& _apvts;
	
	void setFrequency(double newFrequencyHz);

	double frequencyHz;
	double amplitude;
	double phase{0.0};
	double offsetX {0.0};
	double offsetY {0.0};
	double phaseIncrement;	// Phase increment per timer tick
	int updateIntervalMs;
	std::function<void(double, double)> onUpdate;
};

}
