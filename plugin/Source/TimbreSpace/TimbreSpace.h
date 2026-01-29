/*
  ==============================================================================

    TimbreSpace.h
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Features.h"
#include "Statistics.h"
#include "OnsetAnalysis/OnsetAnalysisResult.h"
#include "TimbrePointTypes.h"
#include "../../delaunator-cpp/include/delaunator.hpp"
#include <JuceHeader.h>

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
	std::vector<Timbre5DPoint> const &getTimbreSpacePoints() const;
	std::shared_ptr<analysis::OnsetAnalysisResult> shareOnsets() const;
	//=============================================================================================================================
	void setTimbreSpaceTree(ValueTree const &timbreSpaceTree);
	ValueTree getTimbreSpaceTree() const { return _treeManager.getTimbreSpaceTree(); }
    std::vector<float> getRawFeatureValues(nvs::analysis::Feature_e feature) const;
	//=============================================================================================================================
	bool hasValidAnalysisFor(String const &waveformHash) const;
    String getAudioAbsolutePath() const;
    //=============================================================================================================================
    void setSavePending(const bool saveIsPending) { _analysisSavePending = saveIsPending; }
    bool isSavePending() const { return _analysisSavePending; }
    //=============================================================================================================================
private:
	void valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier &property) override;
	void valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) override;
	void changeListenerCallback(juce::ChangeBroadcaster *source) override; // conditionally updates other state if analyzer has been updated

    void updateDimensionwiseFeatureFromParam(const String& paramID); // updates settings.dimensionwiseFeatures from tree for selected feature and calls fullSelfUpdate
    void updateAllDimensionwiseFeatures();  //  updates settings.dimensionwiseFeatures from tree ALL features. does NOT call any update function.
    void updateHistogramEqualization();
    void updateStatistic();

	struct Settings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Feature_e> dimensionwiseFeatures {
			nvs::analysis::Feature_e::bfcc1,
			nvs::analysis::Feature_e::bfcc2,
			nvs::analysis::Feature_e::bfcc3,
			nvs::analysis::Feature_e::bfcc4,
			nvs::analysis::Feature_e::bfcc5
		};
	    nvs::analysis::Statistic statistic {nvs::analysis::Statistic::Median};
	} settings;
	//=============================================================================================================================
    std::shared_ptr<analysis::OnsetAnalysisResult> _onsetAnalysis;
	//=============================================================================================================================
    class TimbreDataManager {
    public:
        void swapIfPending();

        // audio thread: read current stable data
        const std::vector<Timbre5DPoint>& getTimbreSpacePoints() const;
        void setPoints(const std::vector<Timbre5DPoint>& points); // only gets called downstream from reshape()
        void clear();

        bool isEmpty() const { return _timbres5D.empty(); }
        size_t size() const { return _timbres5D.size(); }

        void setPendingReady();
    private:
        // current stable data (audio thread reads)
        std::vector<Timbre5DPoint> _timbres5D;
        // pending data (message thread writes)
        std::vector<Timbre5DPoint> _timbres5D_pending;
        // for thread synchronization
        std::atomic<bool> _pendingUpdate { false };
    } _timbreDataManager;

    //=============================================================================================================================
    void setPoints(std::vector<Timbre5DPoint> const &points);
    void clearPoints();
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
	
    //=============================================================================================================================
	void signalSaveAnalysisOption() const;
	void signalOnsetsAvailable() const;
    void signalShapedPointsAvailable() const;
    void signalTimbreSpaceTreeChanged() const;
    //=============================================================================================================================

	void fullSelfUpdate(bool verbose); // simply calls the following 3 functions:
	void extractTimbralFeatures(bool verbose=false); // based on settings.dimensionwiseFeatures and settings.statistic, (re)populates _eventwiseExtractedTimbrePoints
	void computeHistogramEqualizedPoints(bool verbose=false); // based on _eventwiseExtractedTimbrePoints, computes _ranges and _histoEqualized dimensions
	void reshape(bool verbose=false); // performs some math such as normalization, squashing, and interpolation (between linear normalized and histogram normalized) on _eventwiseExtractedTimbrePoints (NOT in place) to update _timbreDataManager._timbres5D_pending
    //=============================================================================================================================
    // used only in extractTimbralFeatures(), computeHistogramEqualizedPoints, and reshape()
    struct ExtractedFeatures {
        std::array<std::vector<float>, 5> features {};
        void clearAll() {
            for (auto &feature : features) {
                feature.clear();
            }
        }
        void reserveAll(const size_t numFrames) {
            for (auto &feature : features) {
                feature.reserve(numFrames);
            }
        }
        bool inValidState() const {
            const auto properSize = features[0].size();
            for (size_t i = 1; i < features.size(); ++i) {
                if (features[i].size() != properSize) {
                    return false;
                }
            }
            return true;
        }
        bool allEmpty() const {
            for (const auto &f : features) {
                if (!f.empty()) {
                    return false;
                }
            }
            return true;
        }
        ExtractedFeatures() {clearAll();}
    } _extractedFeatures;
    // std::vector<std::vector<float>> _eventwiseExtractedTimbrePoints;	// gets extracted from _treeManager._timbreSpaceTree any time new view (e.g. different feature set) is requested
    //=============================================================================================================================
    // the following are used in reshape():
    typedef std::pair<float, float> Range;
    std::vector<Range> _ranges {}; // min, max per dimension computed ASAP to efficiently allow histogram equalization
    std::vector<float> _histoEqualizedD0, _histoEqualizedD1 {};
    //=============================================================================================================================
};

}	// namespace nvs::timbrespace
