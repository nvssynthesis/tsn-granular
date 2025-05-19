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


namespace nvs::nav {



class TendencyPointLowpass
{
	// 2-pole lowpass used to smooth tendency point and other changes in an energy-preserving manner
	// for now adapted from https://www.musicdsp.org/en/latest/Filters/27-resonant-iir-lowpass-12db-oct.html
//	resofreq = pole frequency
//	amp = magnitude at pole frequency (approx)

private:
	double _r, _c;
	double sampleRate;
	
	double vibrapos {0.0};
	double vibraspeed {0.0};

public:
	void setSampleRate(double fs);
	
	void setParams(double frequency, double gain);
	
	double operator()(double v);
};

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
	
	TendencyPointLowpass xLP, yLP;

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
	RandomWalk1D(double initial = 0.0, double stepSize = 1e-3)
		: current(initial), stepSize(stepSize), rng(std::random_device{}())
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
		double pUp = (tendency - current) * 0.5 + 0.5;

		// 2) magnitude biased to extremes via sqrt transform
		double u = uni(rng);
		double magnitude = std::sqrt(u) * stepSize;

		// 3) direction
		bool up = (uni(rng) < pUp);
		double accumVal = (up ? magnitude : -magnitude);

		// 4) apply and clamp
		current += accumVal;
		if (current >  1.0) current =  1.0;
		if (current < -1.0) current = -1.0;
		return current;
	}

	void reset(double newValue = 0.0)
	{
		current = newValue;
	}

	double getCurrent() const noexcept { return current; }

	void setTendency(double newTendency)
	{
		tendency = std::clamp(newTendency, -1.0, 1.0);
	}
	
	void setStepSize(double newStepSize)
	{
		jassert ((newStepSize >= 0.0) and (newStepSize <= 1.0));
		stepSize = newStepSize;
	}

private:
	double current;
	double stepSize;
	double tendency;            // bias target in [-1,1]
	std::mt19937 rng;
	std::uniform_real_distribution<double> uni;
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

struct Navigator	:	public juce::ChangeBroadcaster
{
	using ActiveNavigator = std::variant<LFO2D, RandomWalkND>;
	std::function<void(const std::vector<double>&)> onUpdate;
	ActiveNavigator activeNavigator;

	template<typename T, typename... Args>
	Navigator(std::function<void(const std::vector<double>&)> onUpdateFn,
			  std::in_place_type_t<T>,
			  Args&&... args)
	  : onUpdate(std::move(onUpdateFn)),                      // matches declaration order
		activeNavigator(std::in_place_type<T>, std::forward<Args>(args)...)
	{
		passDownOnUpdateFunction();
	}
	
	
	void passDownOnUpdateFunction(){
		std::visit([this](auto &nav){
			jassert (onUpdate != nullptr);
			auto fullOnUpdateFn = [this](const std::vector<double>& v){
				storedPoint = v;
				onUpdate(v);
				sendChangeMessage();
			};
			nav.setOnUpdateCallback(fullOnUpdateFn);
		}, activeNavigator);
	}
	std::vector<double> storedPoint;
};

}
