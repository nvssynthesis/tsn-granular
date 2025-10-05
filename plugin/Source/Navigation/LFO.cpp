/*
  ==============================================================================

    LFO.cpp
    Created: 23 Jan 2025 1:20:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "LFO.h"
#include "../../slicer_granular/Source/Params/params.h"

namespace nvs::nav {



LFO2D::LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz)
    :	_apvts(apvts)
    ,	frequencyHz(*_apvts.getRawParameterValue("nav_lfo_rate"))
    , 	updateIntervalMs(1000.0 / updateRateHz)
{
	setFrequency(*_apvts.getRawParameterValue("nav_lfo_rate"));
	amplitude = (*_apvts.getRawParameterValue("nav_lfo_amount"));
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
	float const paramRate = *_apvts.getRawParameterValue("nav_lfo_rate");
	
	setFrequency(paramRate);
	
	double const filterCutoff = *_apvts.getRawParameterValue("nav_lfo_response");
	double const filterReso = *_apvts.getRawParameterValue("nav_lfo_overshoot");
	xLP.setParams(filterCutoff, filterReso);
	yLP.setParams(filterCutoff, filterReso);
	
	amplitude = *_apvts.getRawParameterValue("nav_lfo_amount");
	
	const double targetX = *_apvts.getRawParameterValue("nav_tendency_x");
	const double targetY = *_apvts.getRawParameterValue("nav_tendency_y");
	
	centerX = targetX;
	centerY = targetY;
	
	// compute phase and waveforms
	assert(phaseIncrement >= 0.0);	// otherwise wrapping method won't work as expected
	phase += phaseIncrement;
	if (phase > 2.0 * juce::MathConstants<double>::pi){
		phase -= 2.0 * juce::MathConstants<double>::pi;
	}
	
	float shapeValue = *_apvts.getRawParameterValue("nav_lfo_shape");
	jassert (shapeValue >= 0.0);
	// https://www.desmos.com/calculator/jnfw9zxkhg
	auto calculate2Dshape = [this, shapeValue](){
		struct P2{ double x, y; };
		
		double a = shapeValue;
		
		auto i = [](double d){return int(d);};
		auto frac = [i](double d){return d - i(d);};
		auto tri = [](double d){ return 2.0 * (d < 0.5 ? d : 1 - d); };
		
		double p = 4.0 * tri(shapeValue);
		double q = 2.0 * a * a;
		
		double x = (1.0 - frac(q)) * std::cos((i(q) + 1.0) * phase) + frac(q) * std::cos((i(q) + 2.0) * phase);
		double y = (1.0 - frac(p)) * std::sin((i(p) + 1.0) * phase) + frac(p) * std::sin((i(p) + 2.0) * phase);
		return P2 {x, y};
	};
	auto const p2 = calculate2Dshape();
	
	double const x = xLP(p2.x * amplitude + centerX);
	double const y = yLP(p2.y * amplitude + centerY);

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
		*_apvts.getRawParameterValue("nav_tendency_x"),
		*_apvts.getRawParameterValue("nav_tendency_y"),
		*_apvts.getRawParameterValue("nav_tendency_z"),
		*_apvts.getRawParameterValue("nav_tendency_u"),
		*_apvts.getRawParameterValue("nav_tendency_v"),
		*_apvts.getRawParameterValue("nav_tendency_w"),
	};
	setTendencyPoint(tendencyPoint);
	
	
	latest.clear();
	latest.reserve(dims);

	float const stepSize = *_apvts.getRawParameterValue("nav_rwalk_step_size");

	for (auto& w : walkers) {
		w.setStepSize(stepSize);
		latest.push_back(w.step());
	}
	if (onUpdate) {
		onUpdate(latest);
	}
}
}	// nvs::nav
