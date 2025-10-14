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
#include <random>	// replace with Xoshirocpp
#include "Synthesis/ResoLowpass.h"

namespace nvs::nav {

class LFO2D : public juce::Timer
{
public:
	LFO2D(juce::AudioProcessorValueTreeState &apvts, double updateRateHz);
	~LFO2D();
	void setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback);

protected:
	void timerCallback() override;

private:
	juce::AudioProcessorValueTreeState& _apvts;
	
	void setFrequency(double newFrequencyHz);
	
	nvs::dsp::ResoLowpass xLP, yLP;

	double frequencyHz;
	double amplitude;
	double centerX = 0.0, centerY = 0.0;    // smoothed center
	double phase {0.0};
	double phaseIncrement;	// Phase increment per timer tick
	double updateIntervalMs;
	std::function<void(const std::vector<double>&)> onUpdate;
};


// A single-dimension random walker with two-step (±stepSize) and state-dependent bias
class RandomWalk1D
{
public:
	RandomWalk1D(const double initial = 0.0, const double stepSize = 1e-3)
		: _current(initial), _stepSize(stepSize), _rng(std::random_device{}())
	{}

	/**
	 * Take one random step:
	 * 1. Compute probability of stepping up: pUp = (1 - current) / 2.
	 * 2. Sample a uniform u in [0,1], set magnitude m = sqrt(u)  // bias towards larger steps
	 * 3. Sample direction: up if uniform < pUp, else down
	 * 4. accumVal = (up? +1 : -1) * m * stepSize
	 * 5. current += accumVal, then clamp to [-1,1]
	 */
	double step()
	{
		// 1) direction bias back toward 0
		const double pUp = (_tendency - _current) * 0.5 + 0.5;

		// 2) magnitude biased to extremes via sqrt transform
		const double u = _uni(_rng);
		const double magnitude = std::sqrt(u) * _stepSize;

		// 3) direction
		const bool up = (_uni(_rng) < pUp);
		const double accumVal = (up ? magnitude : -magnitude);

		// 4) apply and clamp
		_current += accumVal;
		if (_current >  1.0) _current =  1.0;
		if (_current < -1.0) _current = -1.0;
		return _current;
	}

	void reset(const double newValue = 0.0)
	{
		_current = newValue;
	}

	double getCurrent() const noexcept { return _current; }

	void setTendency(const double newTendency)
	{
		_tendency = std::clamp(newTendency, -1.0, 1.0);
	}
	
	void setStepSize(const double newStepSize)
	{
		jassert ((newStepSize >= 0.0) and (newStepSize <= 1.0));
		_stepSize = newStepSize;
	}

private:
	double _current;
	double _stepSize;
	double _tendency;            // bias target in [-1,1]
	std::mt19937 _rng;
	std::uniform_real_distribution<double> _uni;
};

template<typename T>
concept VectorOfDouble = std::same_as<std::remove_cvref_t<T>, std::vector<double>>;	// work for rvalue or lvalue reference


class RandomWalkND : private juce::Timer
{
public:
	RandomWalkND(juce::AudioProcessorValueTreeState &apvts, int dimensions, int rateMs, double stepSize);
	~RandomWalkND() override;

	// Implement this to receive updated N-dimensional positions
	void setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback);

	// Reset all walkers to initial values
	void resetAll(double initialValue = 0.0f);

	void setTendencyPoint(VectorOfDouble auto&& tendencyPoint);
	
private:
	juce::AudioProcessorValueTreeState& _apvts;
	void timerCallback() override;
	
	std::function<void(const std::vector<double>&)> onUpdate;

	int dims;
	std::vector<RandomWalk1D> walkers;
	std::vector<double> latest;
};

struct Navigator//	:	juce::ChangeBroadcaster
{
	using ActiveNavigator = std::variant<LFO2D, RandomWalkND>;

	std::function<void(const std::vector<double>&)> onUpdate;
	ActiveNavigator  activeNavigator;
	std::vector<double> storedPoint;

	template<typename T, typename... Args>
	Navigator(std::function<void(const std::vector<double>&)> onUpdateFn,
			  std::in_place_type_t<T>,
			  Args&&... args)
	  : onUpdate(std::move(onUpdateFn))
	  , activeNavigator(std::in_place_type<T>, std::forward<Args>(args)...)
	{
		passDownOnUpdateFunction();
	}

	void passDownOnUpdateFunction()
	{
		std::visit([this](auto &nav){
			jassert(onUpdate != nullptr);
			auto fullOnUpdateFn = [this](auto const& v){
				storedPoint = v;
				onUpdate(v);
				// sendChangeMessage();
			};
			nav.setOnUpdateCallback(fullOnUpdateFn);
		}, activeNavigator);
	}

	void setLFO2D(juce::AudioProcessorValueTreeState& apvts, float rate=90.f)
	{
		activeNavigator.template emplace<LFO2D>(apvts, rate);	// .template keyword tells compiler we are doing a member template function call, not '<' operator
		passDownOnUpdateFunction();
	}

	void setRandomWalkND(juce::AudioProcessorValueTreeState& apvts, int dimensions=6, int rateMs=10, double stepSize=0.01)
	{
		activeNavigator.template emplace<RandomWalkND>(apvts, dimensions, rateMs, stepSize);
		passDownOnUpdateFunction();
	}
};


}
