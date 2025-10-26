//
// Created by Nicholas Solem on 10/25/25.
//

#pragma once
#include "Navigator.h"

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

}   // namespace nvs::timbrespace