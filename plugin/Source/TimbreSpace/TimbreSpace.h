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

namespace nvs::timbrespace {

class TimbreSpace final :	public juce::ChangeListener
,						    public juce::ValueTree::Listener
,						    public juce::ActionBroadcaster
{
public:
	TimbreSpace(juce::AudioProcessorValueTreeState &apvts);
	~TimbreSpace() override;
	// Delaunator's copy/move ctors/assignment operators are implicitly deleted
	TimbreSpace(const TimbreSpace&) = delete;
	TimbreSpace& operator=(const TimbreSpace& other) = delete;
	TimbreSpace(TimbreSpace&&) noexcept = delete;
	TimbreSpace& operator=(TimbreSpace&&) noexcept = delete;
	//=============================================================================================================================
	void add5DPoint(const Timbre2DPoint &p2D, const Timbre3DPoint &p3D);
	void clearPoints();
	std::vector<Timbre5DPoint> const &getTimbreSpace() const;
    Timbre5DPoint getTargetPoint() const;
	std::vector<util::WeightedIdx> const &getCurrentPointIndices() const;
	std::optional<std::vector<float>> getOnsets() const;
	//=============================================================================================================================
	void setTargetPoint(const Timbre5DPoint& target);
    void computeExistingPointsFromTarget();
	//=============================================================================================================================
	using EventwiseStatisticsF = nvs::analysis::EventwiseStatistics<float>;
	using ValueTree = juce::ValueTree;
	using String = juce::String;
	//=============================================================================================================================
	bool hasValidAnalysisFor(juce::String const &audioHash) const;
	//=============================================================================================================================
	juce::String getAudioAbsolutePath() const { return _audioFileAbsPath; }
	//=============================================================================================================================
	void setTimbreSpaceTree(ValueTree const &timbreSpaceTree);
	ValueTree getTimbreSpaceTree() const { return _treeManager._timbreSpaceTree; }
	//=============================================================================================================================
    void setSavePending(const bool saveIsPending) { _analysisSavePending = saveIsPending; }
    bool isSavePending() const { return _analysisSavePending; }
private:
	void valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier &property) override;
	void valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) override;
	void changeListenerCallback(juce::ChangeBroadcaster *source) override;
	
	struct Settings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Features> dimensionWisefeatures {
			nvs::analysis::Features::bfcc1,
			nvs::analysis::Features::bfcc2,
			nvs::analysis::Features::bfcc3,
			nvs::analysis::Features::bfcc4,
			nvs::analysis::Features::bfcc5
		};
	} settings;
	//=============================================================================================================================
	juce::String _audioFileHash;
	juce::String _audioFileAbsPath;
	//=============================================================================================================================
	std::vector<Timbre5DPoint> _timbres5D;
    Timbre5DPoint _target {};
	std::unique_ptr<delaunator::Delaunator> _delaunator;
	std::vector<util::WeightedIdx> _currentPointIndices {{},{},{}};
	//=============================================================================================================================
	typedef std::pair<float, float> Range;
	std::vector<Range> _ranges {}; // min, max per dimension
	std::vector<float> _histoEqualizedD0, _histoEqualizedD1 {};

	struct TreeManager {
	    explicit TreeManager(AudioProcessorValueTreeState &apvts)
	    : _apvts(apvts)
	    {}
		ValueTree _timbreSpaceTree;
	    AudioProcessorValueTreeState &_apvts;
		var getOnsetsVar() const;
		ValueTree getTimbralFramesTree() const;
		int getNumFrames() const;
	} _treeManager;
	
	bool _analysisSavePending {false};
	
	void signalSaveAnalysisOption() const;
	void signalTimbreSpaceUpdated() const;
	
	std::vector<std::vector<float>> _eventwiseExtractedTimbrePoints;	// gets extracted FROM this->fulltimbreSpace any time new view (e.g. different feature set) is requested
	
	void fullSelfUpdate(bool verbose);	// simply calls the following 3 functions:	
	void extract();
	void updateTimbreSpacePoints();
	void reshape(bool verbose=false);
	//=============================================================================================================================
};

std::vector<util::WeightedIdx> findPointsDistanceBased (const Timbre5DPoint& target,
												    const juce::Array<Timbre5DPoint>&  database,
												    int K,
												    int numToPick,
												    double sharpness,
												    float higher3Dweight);

std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target,
															const std::vector<Timbre5DPoint>& database,
															const delaunator::Delaunator &d);

std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const std::vector<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d);

}	// namespace nvs::timbrespace
