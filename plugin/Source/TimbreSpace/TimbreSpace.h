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

class TimbreSpace final :	public ChangeListener
,						    public ValueTree::Listener
,						    public ActionBroadcaster
{
public:
    explicit TimbreSpace(AudioProcessorValueTreeState &apvts);
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
	void setTimbreSpaceSuperTree(ValueTree const &timbreSpaceSuperTree);
	ValueTree getTimbreSpaceSuperTree() const { return _treeManager.getTimbreSpaceSuperTree(); }
    std::vector<float> getRawFeatureValues(analysis::Feature_e feature,
        std::optional<analysis::Statistic> statToUse=std::nullopt) const;   // if optional arg unspecified, defaults to the stat in settings.statistic
	//=============================================================================================================================
	bool hasValidAnalysisFor(String const &waveformHash) const;
    String getAudioAbsolutePath() const;
    //=============================================================================================================================
    void setSavePending(const bool saveIsPending) { _analysisSavePending = saveIsPending; }
    bool isSavePending() const { return _analysisSavePending; }
    //=============================================================================================================================
private:
	void valueTreePropertyChanged (ValueTree &alteredTree, const Identifier &property) override;
	void valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) override;
	void changeListenerCallback(ChangeBroadcaster *source) override; // conditionally updates other state if analyzer has been updated

    void updateDimensionwiseFeatureFromParam(const String& paramID); // updates settings.dimensionwiseFeatures from tree for selected feature and calls fullSelfUpdate
    void updateAllDimensionwiseFeatures();  //  updates settings.dimensionwiseFeatures from tree ALL features. does NOT call any update function.
    void updateHistogramEqualization();
    void updateStatistic();
    void updateDecorrelate();

	struct Settings {
		float histogramEqualization {0.0f};
		std::vector<analysis::Feature_e> dimensionwiseFeatures {
			analysis::Feature_e::bfcc1,
			analysis::Feature_e::bfcc2,
			analysis::Feature_e::bfcc3,
			analysis::Feature_e::bfcc4,
			analysis::Feature_e::bfcc5
		};
	    analysis::Statistic statistic {analysis::Statistic::Median};
	    bool decorrelateFromPitchAndLoudness {true};
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
		ValueTree getPacmapTree() const;
	    const ValueTree &getTimbreSpaceSuperTree() const;
	    void setTimbreSpaceSuperTree(const ValueTree &timbreSpaceSuperTree);
	    const AudioProcessorValueTreeState &getAPVTS() const { return _apvts; }
		int getNumFrames() const;
	private:
	    ValueTree _timbreSpaceSuperTree; // the most raw, unaffected version of the timbre space data. it gets populated from outside by an Analyzer class.
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
    enum class DimensionalityMode_e {
        Raw,
        Pacmap
    } _dimensionalityMode {DimensionalityMode_e::Pacmap};
    //=============================================================================================================================
	void fullSelfUpdate(bool verbose); // simply calls the following functions:
    void extractTimbralFeatures(bool verbose=false); // based on settings.dimensionwiseFeatures and settings.statistic, (re)populates _eventwiseExtractedTimbrePoints
    void decorrelateFromPitchAndLoudness();
    void computeHistogramEqualizedPoints(bool verbose=false); // based on _eventwiseExtractedTimbrePoints, computes _ranges and _histoEqualized dimensions
	void reshape(bool verbose=false); // performs some math such as normalization, squashing, and interpolation (between linear normalized and histogram normalized) on _eventwiseExtractedTimbrePoints (NOT in place) to update _timbreDataManager._timbres5D_pending
    //=============================================================================================================================
    // the following are used in reshape():
    typedef std::pair<float, float> Range;
    std::vector<Range> _ranges {}; // min, max per dimension computed ASAP to efficiently allow histogram equalization
    std::vector<float> _histoEqualizedD0, _histoEqualizedD1 {};
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
    //=============================================================================================================================
};

}	// namespace nvs::timbrespace
