/*
  ==============================================================================

    LFO.cpp
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LFO.h"
#include "../../slicer_granular/Source/params.h"

namespace nvs::nav {
LFO2D::LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz)
:	_apvts(apvts)
,	frequencyHz(*_apvts.getRawParameterValue("Rate"))
, 	updateIntervalMs(1000.0 / updateRateHz)
{
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	amplitude = (*_apvts.getRawParameterValue("Amount"));
	startTimer(updateIntervalMs);
}

LFO2D::~LFO2D() {
	stopTimer();
}


void LFO2D::setFrequency(double newFrequencyHz) {
	frequencyHz = newFrequencyHz;
	phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequencyHz / (1000.0 / updateIntervalMs);
}

void LFO2D::setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback) {
	onUpdate = std::move(callback);	// not sure if move is going to be smart anymore because navLFOpage now owns function
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
		onUpdate(std::vector<double>({x, y}));
	}
}


RandomWalkND::RandomWalkND(juce::AudioProcessorValueTreeState &apvts, int dimensions, int rateMs, double stepSize)
:	_apvts(apvts),
	dims(dimensions)
{
	walkers.reserve(dims);
	for (int i = 0; i < dims; ++i) {
		walkers.emplace_back(0.0f, stepSize);
	}
	startTimer(rateMs);
}
RandomWalkND::~RandomWalkND() {
	stopTimer();
}

void RandomWalkND::resetAll(double initialValue) {
	for (auto& w : walkers) {
		w.reset(initialValue);
	}
}
void RandomWalkND::setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback){
	onUpdate = std::move(callback);	// not sure if move is going to be smart anymore because navLFOpage now owns function
}

void RandomWalkND::timerCallback() {
	
	latest.clear();
	latest.reserve(dims);

	float const stepSize = *_apvts.getRawParameterValue(getParamName(params_e::nav_random_walk_step_size));

	for (auto& w : walkers) {
		w.setStepSize(stepSize);
		latest.push_back(w.step());
	}
	if (onUpdate) {
		onUpdate(latest);
	}
}
}	// nvs::nav
