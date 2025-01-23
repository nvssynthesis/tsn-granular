/*
  ==============================================================================

    LFO.cpp
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LFO.h"

namespace nvs::nav {
GUILFO::GUILFO(double initialFrequencyHz, double updateRateHz)
:	frequencyHz(initialFrequencyHz), updateIntervalMs(static_cast<int>(1000.0 / updateRateHz))
{
	setFrequency(initialFrequencyHz);
}


void GUILFO::start() {
	startTimer(updateIntervalMs);
}

void GUILFO::stop() {
	stopTimer();
}

void GUILFO::setFrequency(double newFrequencyHz) {
	frequencyHz = newFrequencyHz;
	phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequencyHz / updateIntervalMs;
}

void GUILFO::setOnUpdateCallback(std::function<void(double)> callback) {
	onUpdate = std::move(callback);
}

void GUILFO::timerCallback() {
	phase += phaseIncrement;

	if (phase > 2.0 * juce::MathConstants<double>::pi)
		phase -= 2.0 * juce::MathConstants<double>::pi;

	double lfoValue = std::sin(phase); // Sine wave output (-1 to 1)

	// Trigger the callback to update the TimbreSpaceComponent
	if (onUpdate)
		onUpdate(lfoValue);
}

}	// nvs::nav
