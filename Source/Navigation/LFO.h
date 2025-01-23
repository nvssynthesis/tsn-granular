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
	GUILFO(double initialFrequencyHz, double updateRateHz);
	~GUILFO() override = default;
	void start();
	void stop();
	void setFrequency(double newFrequencyHz);
	void setOnUpdateCallback(std::function<void(double)> callback);

protected:
	void timerCallback() override;

private:
	double frequencyHz{1.0};               // Frequency in Hz
	double phase{0.0};                     // Current phase (radians)
	double phaseIncrement{0.0};            // Phase increment per timer tick
	int updateIntervalMs{16};              // Timer interval in milliseconds (default: ~60 Hz)
	std::function<void(double)> onUpdate;  // Callback to notify updates
};

}
