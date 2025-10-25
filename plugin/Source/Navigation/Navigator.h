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

enum class NavigationType_e {
    Manual = 0,
    LFO,
    RandomWalk,
    Lorenz,
    Hyperchaos
};
typedef nvs::util::Iterator<NavigationType_e, NavigationType_e::Manual, NavigationType_e::Hyperchaos> navigatorTypeIterator;

inline String toString(const NavigationType_e type) {
    switch (type) {
        case NavigationType_e::Manual:
            return "Manual";
        case NavigationType_e::LFO:
            return "LFO";
        case NavigationType_e::RandomWalk:
            return "RandomWalk";
        case NavigationType_e::Lorenz:
            return "Lorenz";
        case NavigationType_e::Hyperchaos:
            return "Hyperchaos";
    }
    return "";
}

inline StringArray getNavigatorTypeArray() {
    static const auto types = [] -> juce::StringArray {
        StringArray result;
        for (const auto it : navigatorTypeIterator()) {
            result.add(toString(it));
        }
        return result;
    }();
    return types;
}

template<typename Point_t>
class NavigationStrategy {
public:
    static constexpr int Dimensions = Point_t::RowsAtCompileTime;

    explicit NavigationStrategy(NavigationType_e type);
    virtual ~NavigationStrategy() = default;

    virtual Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) = 0;
    virtual void setSampleRate(double sampleRate);  // take into account that navigation happens at a lower rate than containing Navigator runs
    NavigationType_e getType() const { return _navType; } // we could also have an internal counter to further ID the queried strategy
protected:
    double _sampleRate {0.0};
    NavigationType_e _navType;
};

template<typename Point_t>
class ManualNavigator final : public NavigationStrategy<Point_t> {
public:
    ManualNavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
    void setSampleRate(double sampleRate) override;
private:
    std::array<dsp::ResoLowpass, NavigationStrategy<Point_t>::Dimensions> filters; // length depends on num dimensions in Point_t
    std::array<double, NavigationStrategy<Point_t>::Dimensions> _centers;
};

template<typename Point_t>
class RandomWalkNavigator final : public NavigationStrategy<Point_t> {
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
    double _phase {0.0}, _phaseIncrement {0.0};
    void setFrequency(double frequency);
};

template<typename Point_t>
class LorenzNavigator final : public NavigationStrategy<Point_t> {
public:
    LorenzNavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
private:
    double _x{0.1}, _y{0.1}, _z{0.1};
};

template<typename Point_t>
class HyperchaosNavigator final : public NavigationStrategy<Point_t> {
public:
    HyperchaosNavigator();
    Point_t navigate(AudioProcessorValueTreeState const &paramTree, TimbreSpace const &space, Point_t previousPoint) override;
private:
    double _x{0.1}, _y{0.1}, _z{0.1}, _u{0.1};
};

template<typename Point_t>
class Navigator final
{
public:
    explicit Navigator(const AudioProcessorValueTreeState& paramTree);

    void setSampleRate(double sampleRate);
    void setNavigationPeriod(double navPeriodMs);
    void setNavigationStrategy(NavigationType_e navType);
    NavigationType_e getNavigationStrategy() const {
        if (_navigationStrategy) {
            return _navigationStrategy->getType();
        }
        return static_cast<NavigationType_e>(-1);
    }

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


// NavigationManager.h
namespace nvs::timbrespace {
template<typename Point_t>
class NavigationManager {
public:
    explicit NavigationManager(const AudioProcessorValueTreeState& paramTree)
    :   _apvts(paramTree),
        _autoNavigator(paramTree),
        _manualNavigator(paramTree)
    {
        _manualNavigator.setNavigationStrategy(NavigationType_e::Manual);
        _navigators[0] = &_autoNavigator;
        _navigators[1] = &_manualNavigator;
    }
    void setSampleRate(double sampleRate) {
        for (auto *nav : _navigators) {
            nav->setSampleRate(sampleRate);
        }
    }
    void setNavigationPeriod(double navPeriodMs) {
        for (auto *nav : _navigators) {
            nav->setNavigationPeriod(navPeriodMs);
        }
    }
    void setNavigationStrategy(NavigationType_e navType) {
        _autoNavigator.setNavigationStrategy(navType);  // manual is always using NavigationType_e::Manual
    }
    NavigationType_e getNavigationStrategy() const {
        return _autoNavigator.getNavigationStrategy();
    }
    Point_t process(TimbreSpace const &space, int numSamplesElapsed);

private:
    const juce::AudioProcessorValueTreeState &_apvts;
    Navigator<Point_t> _autoNavigator;
    Navigator<Point_t> _manualNavigator;

    std::array<Navigator<Point_t>*, 2> _navigators;
};


template<>
inline
Timbre5DPoint NavigationManager<Timbre5DPoint>::process(TimbreSpace const &space, int numSamplesElapsed) {
    const double scaling = *_apvts.getRawParameterValue("nav_scaling");

    Timbre5DPoint pAuto = scaling * _autoNavigator.process(space, numSamplesElapsed);

    const Eigen::Vector3f p3D = pAuto.head<3>();
    const Eigen::Vector2f p2D = pAuto.tail<2>();

    const Eigen::AngleAxisf rot_x((*_apvts.getRawParameterValue("nav_rotation_x")) * 2.0 * M_PI, Eigen::Vector3f::UnitX());
    const Eigen::AngleAxisf rot_y((*_apvts.getRawParameterValue("nav_rotation_y")) * 2.0 * M_PI, Eigen::Vector3f::UnitY());
    const Eigen::AngleAxisf rot_z((*_apvts.getRawParameterValue("nav_rotation_z")) * 2.0 * M_PI, Eigen::Vector3f::UnitZ());

    pAuto << rot_z * rot_y * rot_x * p3D, p2D;

    const auto pManual = _manualNavigator.process(space, numSamplesElapsed);

    return pManual + pAuto;
}

}   // nnamespace nvs::timbrespace