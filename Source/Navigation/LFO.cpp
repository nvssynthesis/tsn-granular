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

void TendencyPointLowpass::setSampleRate(double fs){
	sampleRate = fs;
}

void TendencyPointLowpass::setParams(double fc, double gain){
	double const w = 2.0 * juce::MathConstants<double>::pi * fc / sampleRate; // Pole angle
	double const q = 1.0 - w / (2.0 * (gain + 0.5 / (1.0 + w)) + w - 2.0); // Pole magnitude
	_r = q * q;
	_c = _r + 1.0 - 2.0 * cos(w) * q;
}

double TendencyPointLowpass::operator()(double v){
	/* Accelerate vibra by signal-vibra, multiplied by lowpasscutoff */
	vibraspeed += (v - vibrapos) * _c;
	
	/* Add velocity to vibra's position */
	vibrapos += vibraspeed;
	
	/* Attenuate/amplify vibra's velocity by resonance */
	vibraspeed *= _r;
	
	/* Check clipping */
	double tmp = vibrapos;
	tmp = juce::jlimit(-32768.0, 32767.0, tmp);
//	tmp *= (1.0 / 32767.0);
	
	return tmp;
}


LFO2D::LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz)
:	_apvts(apvts)
,	frequencyHz(*_apvts.getRawParameterValue("Rate"))
, 	updateIntervalMs(1000.0 / updateRateHz)
{
	setFrequency(*_apvts.getRawParameterValue("Rate"));
	amplitude = (*_apvts.getRawParameterValue("Amount"));
	startTimer(updateIntervalMs);
	double sr = 1.0 / (getTimerInterval() / 1000.0);
	xLP.setSampleRate(sr);
	yLP.setSampleRate(sr);
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
	float const paramRate = *_apvts.getRawParameterValue(getParamName(params_e::nav_lfo_2d_rate));
	
	setFrequency(paramRate);
	
	double const filterCutoff = *_apvts.getRawParameterValue(getParamName(params_e::nav_lfo_2d_response));
	double const filterReso = *_apvts.getRawParameterValue(getParamName(params_e::nav_lfo_2d_overshoot));
	xLP.setParams(filterCutoff, filterReso);
	yLP.setParams(filterCutoff, filterReso);
	
	amplitude = *_apvts.getRawParameterValue(getParamName(params_e::nav_lfo_2d_amount));
	
	const double targetX = *_apvts.getRawParameterValue(getParamName(params_e::nav_tendency_x));
	const double targetY = *_apvts.getRawParameterValue(getParamName(params_e::nav_tendency_y));
	
	centerX = targetX;
	centerY = targetY;
	
	// compute phase and waveforms
	assert(phaseIncrement >= 0.0);	// otherwise wrapping method won't work as expected
	phase += phaseIncrement;
	if (phase > 2.0 * juce::MathConstants<double>::pi){
		phase -= 2.0 * juce::MathConstants<double>::pi;
	}
	
	double const x = xLP(std::cos(phase) * amplitude + centerX);
	double const y = yLP(std::sin(phase) * amplitude + centerY);

	// Trigger the callback to update the TimbreSpaceComponent
	if (onUpdate){
		onUpdate(std::vector<double>({x, y}));
	}
}


RandomWalkND::RandomWalkND(juce::AudioProcessorValueTreeState &apvts, int dimensions, int rateMs, double stepSize)
:	_apvts(apvts)
,	dims(dimensions)
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
void RandomWalkND::setTendencyPoint(VectorOfDouble auto&& tendencyPoint)
{
	size_t len = std::min(walkers.size(), tendencyPoint.size());
	for (size_t i = 0; i < len; ++i)
		walkers[i].setTendency(tendencyPoint[i]);
}

void RandomWalkND::setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback){
	onUpdate = std::move(callback);	// not sure if move is going to be smart anymore because navLFOpage now owns function
}

void RandomWalkND::timerCallback() {
	std::vector<double> tendencyPoint {
		*_apvts.getRawParameterValue("Tendency X"),
		*_apvts.getRawParameterValue("Tendency Y"),
		*_apvts.getRawParameterValue("Tendency Z"),
		*_apvts.getRawParameterValue("Tendency U"),
		*_apvts.getRawParameterValue("Tendency V"),
		*_apvts.getRawParameterValue("Tendency W"),
	};
	setTendencyPoint(tendencyPoint);
	
	
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
