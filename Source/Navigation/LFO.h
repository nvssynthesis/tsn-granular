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
	void setOnUpdateCallback(std::function<void(double, double)> callback);

protected:
	void timerCallback() override;

private:
	double frequencyHz{1.0};
	double phase{0.0};
	double phaseIncrement{0.0};            // Phase increment per timer tick
	int updateIntervalMs{16};
	std::function<void(double, double)> onUpdate;
};

}
