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
#include "../Analysis/ThreadedAnalyzer.h"   // just for OnsetAnalysisResult
#include "../../slicer_granular/Source/misc_util.h"
#include "TimbrePointTypes.h"
#include "../../delaunator-cpp/include/delaunator.hpp"

namespace nvs::timbrespace {

class TimbreSpace final :	public juce::ChangeListener
,						    public juce::ValueTree::Listener
,						    public juce::ActionBroadcaster
{
public:
    explicit TimbreSpace(juce::AudioProcessorValueTreeState &apvts);
	~TimbreSpace() override;
	// Delaunator's copy/move ctors/assignment operators are implicitly deleted
	TimbreSpace(const TimbreSpace&) = delete;
	TimbreSpace& operator=(const TimbreSpace& other) = delete;
	TimbreSpace(TimbreSpace&&) noexcept = delete;
	TimbreSpace& operator=(TimbreSpace&&) noexcept = delete;
	//=============================================================================================================================
	std::vector<Timbre5DPoint> const &getTimbreSpace() const;
    Timbre5DPoint getTargetPoint() const;
	std::vector<util::WeightedIdx> const &getCurrentPointIndices() const;
	std::shared_ptr<analysis::ThreadedAnalyzer::OnsetAnalysisResult> shareOnsets() const;
	//=============================================================================================================================
	[[deprecated]] void setTargetPoint(const Timbre5DPoint& target);
    [[deprecated]] void computeExistingPointsFromTarget();
	//=============================================================================================================================
	using EventwiseStatisticsF = nvs::analysis::EventwiseStatistics<float>;
	using ValueTree = juce::ValueTree;
	using String = juce::String;
	//=============================================================================================================================
	bool hasValidAnalysisFor(juce::String const &waveformHash) const;
	//=============================================================================================================================
	juce::String getAudioAbsolutePath() const;
	//=============================================================================================================================
	void setTimbreSpaceTree(ValueTree const &timbreSpaceTree);
	ValueTree getTimbreSpaceTree() const { return _treeManager.getTimbreSpaceTree(); }
	//=============================================================================================================================
    void setSavePending(const bool saveIsPending) { _analysisSavePending = saveIsPending; }
    bool isSavePending() const { return _analysisSavePending; }
private:
	void valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier &property) override;
	void valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) override;
	void changeListenerCallback(juce::ChangeBroadcaster *source) override; // conditionally calls analyzerUpdated
    void analyzerUpdated(nvs::analysis::ThreadedAnalyzer &a);

    void updateDimensionwiseFeature(const juce::String& paramID); // updates settings.dimensionwiseFeatures from tree for selected feature and calls fullSelfUpdate
    void updateAllDimensionwiseFeatures();  //  updates settings.dimensionwiseFeatures from tree ALL features. does NOT call any update function.
    void updateStatistic();

	struct Settings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Features> dimensionwiseFeatures {
			nvs::analysis::Features::bfcc1,
			nvs::analysis::Features::bfcc2,
			nvs::analysis::Features::bfcc3,
			nvs::analysis::Features::bfcc4,
			nvs::analysis::Features::bfcc5
		};
	    nvs::analysis::Statistic statistic {nvs::analysis::Statistic::Median};
	} settings;
	//=============================================================================================================================
    std::shared_ptr<analysis::ThreadedAnalyzer::OnsetAnalysisResult> _onsetAnalysis;
	//=============================================================================================================================
    class TimbreDataManager {
    public:
        void triangulatePendingData(bool verbose=false);
        void swapIfPending(bool verbose=false);

        // Audio thread: read current stable data
        const std::vector<Timbre5DPoint>& getTimbres() const;
        void setPoints(const std::vector<Timbre5DPoint>& points); // only gets called downstream from reshape()
        void clear();
        bool isReadyForTriangulation() const;

        const delaunator::Delaunator* getDelaunator() const {
            return _delaunator.get();
        }

        bool isEmpty() const {
            return _timbres5D.empty();
        }

        size_t size() const {
            return _timbres5D.size();
        }

    private:
        // Current stable data (audio thread reads)
        std::vector<Timbre5DPoint> _timbres5D;
        std::unique_ptr<delaunator::Delaunator> _delaunator;

        // Pending data (message thread writes)
        std::vector<Timbre5DPoint> _timbres5D_pending;
        std::unique_ptr<delaunator::Delaunator> _delaunator_pending;

        std::atomic<bool> _pendingUpdate { false };
    } _timbreDataManager;
    Timbre5DPoint _target {};
	std::vector<util::WeightedIdx> _currentPointIndices {{},{},{}};
    //=============================================================================================================================
    void setPoints(std::vector<Timbre5DPoint> const &points);
    void clearPoints();
	//=============================================================================================================================
	// the following are used in reshape():
	typedef std::pair<float, float> Range;
	std::vector<Range> _ranges {}; // min, max per dimension computed ASAP to efficiently allow histogram equalization
	std::vector<float> _histoEqualizedD0, _histoEqualizedD1 {};
    //=============================================================================================================================
	class TreeManager {
	public:
	    TreeManager(AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace);
	    ~TreeManager();
		var getOnsetsVar() const;
		ValueTree getTimbralFramesTree() const;
	    const ValueTree &getTimbreSpaceTree() const;
	    void setTimbreSpaceTree(ValueTree timbreSpaceTree);
	    const AudioProcessorValueTreeState &getAPVTS() const { return _apvts; }
		int getNumFrames() const;
	private:
	    ValueTree _timbreSpaceTree; // the most raw, unaffected version of the timbre space data. it gets populated from outside by an Analyzer class.
	    AudioProcessorValueTreeState &_apvts;
	    TimbreSpace &_timbreSpace;  // just for adding/removing as listener
	} _treeManager;
	
	bool _analysisSavePending {false};
	
	void signalSaveAnalysisOption() const;
	void signalOnsetsAvailable() const;
	
	std::vector<std::vector<float>> _eventwiseExtractedTimbrePoints;	// gets extracted from _treeManager._timbreSpaceTree any time new view (e.g. different feature set) is requested
	
	void fullSelfUpdate(bool verbose); // simply calls the following 3 functions:
	void extractTimbralFeatures(bool verbose=false); // based on settings.dimensionwiseFeatures and settings.statistic, (re)populates _eventwiseExtractedTimbrePoints
	void computeHistogramEqualizedPoints(bool verbose=false); // based on _eventwiseExtractedTimbrePoints, computes _ranges and _histoEqualized dimensions
	void reshape(bool verbose=false); // performs some math such as normalization, squashing, and interpolation (between linear normalized and histogram normalized) on _eventwiseExtractedTimbrePoints (NOT in place) to update _timbreDataManager._timbres5D_pending
};

//=============================================================================================================================
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
