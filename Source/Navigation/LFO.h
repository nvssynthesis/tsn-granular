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


class RandomWalk1D
{
public:
	RandomWalk1D(double initial = 0.0f, float stepSize = 1.0f)
		: current(initial),
		  rng(std::random_device{}()), dist(-stepSize, stepSize)
	{}

	float step() {
		current += dist(rng);
		return current;
	}

	void reset(double newValue = 0.0f) {
		current = newValue;
	}

	float getCurrent() const noexcept { return current; }

private:
	double current;
	std::mt19937 rng;
	std::uniform_real_distribution<double> dist;
};

class RandomWalkND : private juce::Timer
{
public:
	RandomWalkND(int dimensions, int rateMs, double stepSize);
	~RandomWalkND() override;

	// Implement this to receive updated N-dimensional positions
	void setOnUpdateCallback(std::function<void(const std::vector<double>&)> callback);

	// Reset all walkers to initial values
	void resetAll(double initialValue = 0.0f);

private:
	void timerCallback() override;
	
	std::function<void(const std::vector<double>&)> onUpdate;

	int dims;
	std::vector<RandomWalk1D> walkers;
	std::vector<double> latest;
};


using Navigator = std::variant<LFO2D, RandomWalkND>;

}
