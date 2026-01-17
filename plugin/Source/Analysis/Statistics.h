/*
  ==============================================================================

    Statistics.h
    Created: 2 Jul 2025 8:35:27pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "../../slicer_granular/Source/misc_util.h"

namespace nvs::analysis {

enum class Statistic {
	Mean,
	Median,
	Variance,
	Skewness,
	Kurtosis,
	NumStatistics
};
typedef nvs::util::Iterator<Statistic, Statistic::Mean, Statistic::Kurtosis> statisticIterator;

template <typename T>
struct EventwiseStatistics {
	T mean		{};
	T median	{};
	T variance	{};
	T skewness	{};
	T kurtosis	{};
};
}
