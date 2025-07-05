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
#include "TimbrePointTypes.h"
#include "../../delaunator-cpp/include/delaunator.hpp"

//namespace delaunator {
//class Delaunator;
//}
namespace nvs::timbrespace {

class TimbreSpace	:	public juce::ChangeListener,	public juce::ValueTree::Listener,	public juce::ChangeBroadcaster
{
public:
	TimbreSpace();
	// Declare but don't define these in the header; otherwise we need the full Delaunator definition available
	~TimbreSpace();
	// Delaunator's copy/move ctors/assignment operators are implicitly deleted
	TimbreSpace(const TimbreSpace&) = delete;
	TimbreSpace& operator=(const TimbreSpace& other) = delete;
	TimbreSpace(TimbreSpace&&) noexcept = delete;
	TimbreSpace& operator=(TimbreSpace&&) noexcept = delete;
	
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
	std::unique_ptr<delaunator::Delaunator> _delaunator;
	
	std::vector<util::WeightedIdx> currentPointIndices {{},{},{},{}};
	
	//=============================================================================================================================
	// these once belonged in TimbreSpaceNeededData, which was silly design
	typedef std::pair<float, float> Range;
	std::vector<Range> _ranges {}; // min, max per dimension
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
