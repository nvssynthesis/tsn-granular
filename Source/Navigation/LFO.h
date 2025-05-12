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

class LFO2D : public juce::Timer
{
public:
	LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz);
	~LFO2D() override = default;
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
	double updateIntervalMs;
	std::function<void(double, double)> onUpdate;
};

}
