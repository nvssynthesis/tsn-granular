/*
  ==============================================================================

    LFO.cpp
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LFO.h"

namespace nvs::nav {
LFO2D::LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz)
:	_apvts(apvts)
,	frequencyHz(*_apvts.getRawParameterValue("Rate"))
, 	updateIntervalMs(1000.0 / updateRateHz)
{
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	amplitude = (*_apvts.getRawParameterValue("Amount"));
}

void LFO2D::start() {
	startTimer(updateIntervalMs);
}

void LFO2D::stop() {
	stopTimer();
}

void LFO2D::setFrequency(double newFrequencyHz) {
	frequencyHz = newFrequencyHz;
	phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequencyHz / (1000.0 / updateIntervalMs);
}

void LFO2D::setOnUpdateCallback(std::function<void(double, double)> callback) {
	onUpdate = std::move(callback);
}

void LFO2D::timerCallback() {
	// update parameters
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	amplitude = *_apvts.getRawParameterValue("Amount");
	offsetX = *_apvts.getRawParameterValue("X Offset");
	offsetY = *_apvts.getRawParameterValue("Y Offset");

	// compute phase and waveforms
	assert(phaseIncrement >= 0.0);	// otherwise wrapping method won't work as expected
	phase += phaseIncrement;

	if (phase > 2.0 * juce::MathConstants<double>::pi){
		phase -= 2.0 * juce::MathConstants<double>::pi;
	}
	double const x = std::cos(phase) * amplitude + offsetX;
	double const y = std::sin(phase) * amplitude + offsetY;

	// Trigger the callback to update the TimbreSpaceComponent
	if (onUpdate){
		onUpdate(x, y);
	}
}

}	// nvs::nav
