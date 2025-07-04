/*
  ==============================================================================

    TimbreSpaceHolder.h
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../Analysis/Features.h"
#include "../Analysis/Statistics.h"
#include "../../slicer_granular/Source/misc_util.h"

namespace nvs::timbrespace {

using timbre2DPoint = juce::Point<float>;
using timbre3DPoint = std::array<float, 3>;

struct Timbre5DPoint {
	
	timbre2DPoint _p2D;			// used to locate the point in x,y plane
	timbre3DPoint _p3D;	// used to describe the colour (hsv)
	
	bool operator==(Timbre5DPoint const &other) const;
	timbre2DPoint get2D() const { return _p2D; }
	timbre3DPoint get3D() const { return _p3D; }
	
	// to easily trade hsv for rbg
	std::array<juce::uint8, 3> toUnsigned() const;
};


class TimbreSpace	:	public juce::ChangeListener,	public juce::ValueTree::Listener,	public juce::ChangeBroadcaster
{
public:
	void add5DPoint(timbre2DPoint p2D, std::array<float, 3> p3D);
	void clear();
	
	juce::Array<Timbre5DPoint> &getTimbreSpace() {
		return timbres5D;
	}
	std::vector<util::WeightedIdx> getCurrentPointIndices() const {
		return currentPointIndices;
	}
	void setProbabilisticPointFromTarget(const Timbre5DPoint& target, int K_neighbors, double sharpness, float higher3Dweight=0.f);
	
	//=============================================================================================================================
	// these once belonged in TimbreSpaceNeededData, which was silly design

	using EventwiseStatisticsF = nvs::analysis::EventwiseStatistics<float>;
	

	void valueTreePropertyChanged (juce::ValueTree &alteredTree, const juce::Identifier &property) override;
	void changeListenerCallback(juce::ChangeBroadcaster *source) override;
	//=============================================================================================================================

private:
	struct Settings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Features> dimensionWisefeatures {
			nvs::analysis::Features::bfcc1,
			nvs::analysis::Features::bfcc2,
			nvs::analysis::Features::bfcc3,
			nvs::analysis::Features::bfcc4,
			nvs::analysis::Features::bfcc5
		};
	};
	Settings settings;

	juce::Array<Timbre5DPoint> timbres5D;
	
	
	std::vector<util::WeightedIdx> currentPointIndices {{},{},{},{}};
	
	//=============================================================================================================================
	// these once belonged in TimbreSpaceNeededData, which was silly design
	typedef std::pair<float, float> Range;
	std::vector<Range> ranges {}; // min, max per dimension
	std::vector<float> histoEqualizedD0, histoEqualizedD1 {};

	std::optional<std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>>> fullTimbreSpace;	// gets stolen FROM analyzer (saves significant memory)
	std::vector<std::vector<float>> eventwiseExtractedTimbrePoints;	// gets extracted FROM this->fulltimbreSpace any time new view (e.g. different feature set) is requested

	
	void updateTimbreSpacePoints();
	void extract();
	//=============================================================================================================================
	// this once was a free function, but now it only is about changing stuff internal to TimbreSpace, not communicating between 2 coupled classes
	void reshape(bool verbose=false);
	//=============================================================================================================================
};

std::vector<util::WeightedIdx> findWeightedPoints (
	const Timbre5DPoint&               target,
	const juce::Array<Timbre5DPoint>&  database,
	int                                K,
	int                                numToPick,
	double                             sharpness,
	float                              higher3Dweight);

}	// namespace nvs::timbrespace
