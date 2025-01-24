/*
  ==============================================================================

    LFO.cpp
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LFO.h"

namespace nvs::nav {
GUILFO::GUILFO(juce::AudioProcessorValueTreeState &apvts, double updateRateHz)
:	_apvts(apvts)
,	frequencyHz(*_apvts.getRawParameterValue("Rate"))
, 	updateIntervalMs(static_cast<int>(1000.0 / updateRateHz))
{
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	setAmplitude(*_apvts.getRawParameterValue("Amount"));
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
void GUILFO::setAmplitude(double newAmplitude) {
	amplitude = newAmplitude;
}

void GUILFO::setOnUpdateCallback(std::function<void(double, double)> callback) {
	onUpdate = std::move(callback);
}

void GUILFO::timerCallback() {
	// update parameters
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	setAmplitude(*_apvts.getRawParameterValue("Amount"));
	
	// compute phase and waveforms
	assert(phaseIncrement >= 0.0);	// otherwise wrapping method won't work as expected
	phase += phaseIncrement;

	if (phase > 2.0 * juce::MathConstants<double>::pi){
		phase -= 2.0 * juce::MathConstants<double>::pi;
	}
	double const x = std::cos(phase) * amplitude;
	double const y = std::sin(phase) * amplitude;

	// Trigger the callback to update the TimbreSpaceComponent
	if (onUpdate)
		onUpdate(x, y);
}

}	// nvs::nav
