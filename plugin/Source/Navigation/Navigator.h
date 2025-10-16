/*
  ==============================================================================

    Navigator.h
    Created: 26 Sep 2025 1:59:37am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "TimbreSpace/TimbreSpace.h"
#include "Synthesis/ResoLowpass.h"
#include <random>

namespace nvs::timbrespace {

template<typename Point_t>
class NavigationStrategy {
public:
    static constexpr int Dimensions = Point_t::RowsAtCompileTime;
    NavigationStrategy(String name);
    virtual ~NavigationStrategy() = default;

    virtual Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) = 0;
    virtual void setSampleRate(double sampleRate);  // take into account that navigation happens at a lower rate than containing Navigator runs
    virtual String getName() const { return _name; } // we could also have an internal counter to further ID the queried strategy
protected:
    double _sampleRate {0.0};
    String _name;
};

template<typename Point_t>
class RandomWalkNavigator : public NavigationStrategy<Point_t> {
public:
    RandomWalkNavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
private:
#pragma message("use instead xoshiro")
    std::mt19937 _rng;
    std::uniform_real_distribution<float> _uni;
};

template<typename Point_t>
class LFONavigator : public NavigationStrategy<Point_t> {
public:
    LFONavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
private:
    std::vector<nvs::dsp::ResoLowpass> filters; // length depends on num dimensions in Point_t
};

template<typename Point_t>
class LorenzNavigator : public NavigationStrategy<Point_t> {
public:
    LorenzNavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
};

enum class NavigationType_e {
    LFO = 0,
    RandomWalk,
    Lorenz
};

inline const juce::StringArray &getNavigationTypeNames() {
    static const auto names = []() -> juce::StringArray {
        return {"LFO", "RandomWalk", "Lorenz"};
    }();
    return names;
}

template<typename Point_t>
class Navigator final
{
public:
    explicit Navigator(const AudioProcessorValueTreeState& paramTree);

    void setSampleRate(double sampleRate);
    void setNavigationPeriod(double navPeriodMs);
    void setNavigationStrategy(NavigationType_e navType);

    // client calls this samplewise, while this calls proper navigation function periodically based on navRate and sampleRate
    Point_t process(TimbreSpace const &space, int numSamplesElapsed);

private:
    std::unique_ptr<NavigationStrategy<Point_t>> _navigationStrategy;
    juce::AudioProcessorValueTreeState const &_apvts;
    Point_t _previousPoint {};

    void updateNavPeriodSamples();
    void updateStrategyRate();
    double _sampleRate {0}, _navPeriodMs {0};
    int _sampleCounter {0}, _navPeriodSamps {0};
};
}

#include "Navigator.tpp"
