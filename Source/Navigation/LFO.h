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

	double frequencyHz;
	double amplitude;
	double phase {0.0};
	double offsetX {0.0};
	double offsetY {0.0};
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

	/*
	Take one biased random step:
	 - Probability to step +stepSize = (1 - current)/2
	 - Probability to step -stepSize = (1 + current)/2
	This keeps the walker heuristically within [-1,1].
	 */
	double step()
	{
		// compute up-step probability based on current position
		double pUp = (1.0 - current) * 0.5;
		jassert ((pUp >= 0.0) and (pUp <= 1.0));
		// uniform real [0,1)
		std::uniform_real_distribution<double> uni(0.0, 1.0);
		bool moveUp = (uni(rng) < pUp);

		current += moveUp ? stepSize : -stepSize;

		// clamp to [-1,1] for safety
		if (current > 1.0)  current = 1.0;
		if (current < -1.0) current = -1.0;

		return current;
	}

	void reset(double newValue = 0.0)
	{
		current = newValue;
	}

	double getCurrent() const noexcept { return current; }

	void setStepSize(double newStepSize)
	{
		jassert ((newStepSize >= 0.0) and (newStepSize <= 1.0));
		stepSize = newStepSize;
	}

private:
	double current;
	double stepSize;
	std::mt19937 rng;
};

class RandomWalkND : private juce::Timer
{
public:
	RandomWalkND(juce::AudioProcessorValueTreeState &apvts, int dimensions, int rateMs, double stepSize);
	~RandomWalkND() override;

	// Implement this to receive updated N-dimensional positions
	void setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback);

	// Reset all walkers to initial values
	void resetAll(double initialValue = 0.0f);

private:
	juce::AudioProcessorValueTreeState& _apvts;
	void timerCallback() override;
	
	std::function<void(const std::vector<double>&)> onUpdate;

	int dims;
	std::vector<RandomWalk1D> walkers;
	std::vector<double> latest;
};


using Navigator = std::variant<LFO2D, RandomWalkND>;

}
