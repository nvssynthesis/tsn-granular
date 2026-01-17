/*
  ==============================================================================

    TimbrePointTypes.h
    Created: 4 Jul 2025 8:32:06pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <Eigen/Dense>

namespace nvs::timbrespace {

template<int Dimensions>
using TimbrePoint = Eigen::Vector<float, Dimensions>;

using Timbre2DPoint = TimbrePoint<2>;
using Timbre3DPoint = TimbrePoint<3>;
using Timbre4DPoint = TimbrePoint<4>;
using Timbre5DPoint = TimbrePoint<5>;

inline Timbre2DPoint get2D(Timbre5DPoint p) {
    return Timbre2DPoint{p[0], p[1]};
}
inline Timbre3DPoint get3D(Timbre5DPoint p) {
    return Timbre3DPoint{p[2], p[3], p[4]};
}
inline Timbre5DPoint to5D(Timbre2DPoint p2, Timbre3DPoint p3) {
    return Timbre5DPoint{p2[0], p2[1], p3[0], p3[1], p3[2]};
}

using uint8 = unsigned char;

std::array<uint8, 3> toUnsigned(Timbre3DPoint p);


bool inRange0_1(const auto& point) {
    return (point.array() >= 0.f).all() && (point.array() <= 1.f);
}
bool inRangeM1_1(const auto& point) {
    return (point.array().abs() <= 1.0f).all();
}

static constexpr bool inRange0_1(float x){
	return ( (x >= 0.f) && (x <= 1.f) );
}
static constexpr bool inRangeM1_1(float x){
	return ( (x >= -1.f) && (x <= 1.f) );
}

template<int N>
struct WrappedPoint {
    TimbrePoint<N> point;
    bool active { true };
};

}	// namespace nvs::timbrespace
