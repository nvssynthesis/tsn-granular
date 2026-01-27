/*
  ==============================================================================

    TimbreSpace.cpp
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpace.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "../../slicer_granular/Source/misc_util_juce.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "Analysis/FeatureOperations.h"
#include <ranges>
#include "fmt/core.h"
#include "StringAxiom.h"
#include "dsp_util.h"

namespace nvs::timbrespace {


//===============================================TimbreSpace=================================================

TimbreSpace::TimbreSpace(juce::AudioProcessorValueTreeState &apvts)
    :   _treeManager(apvts, *this)
{}
TimbreSpace::~TimbreSpace() {}

void TimbreSpace::setPoints(std::vector<Timbre5DPoint> const &points){
	using namespace nvs::util;
    assert(std::ranges::all_of(points,
        [](const auto& p) {
            return p.cwiseAbs().maxCoeff() <= 1.0;
        }));
	_timbreDataManager.setPoints(points);
}
void TimbreSpace::clearPoints() {
	_timbreDataManager.clear();
}
void TimbreSpace::TimbreDataManager::setPoints(const std::vector<Timbre5DPoint> &points) {
    _timbres5D_pending = points;
}
void TimbreSpace::TimbreDataManager::clear() {
    _timbres5D_pending.clear();
}
void TimbreSpace::TimbreDataManager::setPendingReady() {
    if (_timbres5D_pending.empty()) {
        return;
    }
    _pendingUpdate.store(true, std::memory_order_release);
}
std::vector<Timbre5DPoint> const &TimbreSpace::getTimbreSpacePoints() const {
    return _timbreDataManager.getTimbreSpacePoints();
}

const std::vector<Timbre5DPoint> &TimbreSpace::TimbreDataManager::getTimbreSpacePoints() const {
    return _timbres5D;
}

String TimbreSpace::getAudioAbsolutePath() const {
    if (_onsetAnalysis == nullptr) {
        return "";
    }
    return _onsetAnalysis->audioFileAbsPath;
}
auto TimbreSpace::shareOnsets() const -> std::shared_ptr<analysis::OnsetAnalysisResult>  {
    return _onsetAnalysis;
}
bool TimbreSpace::hasValidAnalysisFor(juce::String const &waveformHash) const {
    if (_onsetAnalysis == nullptr) {
        return false;
    }
	return _onsetAnalysis->waveformHash == waveformHash;
}

static const std::map<juce::String, size_t> pidToDimensionMap {
    {nvs::axiom::x_axis, 0},
    {nvs::axiom::y_axis, 1},
    {nvs::axiom::z_axis, 2},
    {nvs::axiom::u_axis, 3},
    {nvs::axiom::v_axis, 4},
};

void TimbreSpace::updateDimensionwiseFeatureFromParam(const juce::String& paramID) {
    for (auto const &[s, i] : pidToDimensionMap) {
        if (paramID == s) {
            settings.dimensionwiseFeatures[i] = static_cast<nvs::analysis::Feature_e>(_treeManager.getAPVTS().getRawParameterValue(s)->load());
            fullSelfUpdate(false);
            return;
        }
    }
}
void TimbreSpace::updateAllDimensionwiseFeatures(){
    for (auto const &[s, i] : pidToDimensionMap) {
        const auto val = _treeManager.getAPVTS().getRawParameterValue(s)->load();
        const auto feat = static_cast<nvs::analysis::Feature_e>(val);
        settings.dimensionwiseFeatures[i] = feat;
    }
}
//=============================================================================================================================
void TimbreSpace::valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier & property) {
    if (alteredTree.hasType(nvs::axiom::PARAM) && property == juce::Identifier("value")) {
        const auto paramID = alteredTree["id"].toString();
        const float newValue = alteredTree["value"];

        if (paramID == nvs::axiom::histogram_equalization) {
            DBG("Histogram equalization changed to: " + juce::String(newValue));
            updateHistogramEqualization();
            DBG("tree changed! redrawing points...\n");
            fullSelfUpdate(false);
            return;
        }
        if (paramID == nvs::axiom::statistic) {
            updateStatistic();
            DBG("tree changed! redrawing points...\n");
            fullSelfUpdate(false);
            return;
        }
        updateDimensionwiseFeatureFromParam(paramID);
    }
}
void TimbreSpace::valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) {
    if (&treeWhichHasBeenChanged == &_treeManager.getTimbreSpaceTree()){
        signalOnsetsAvailable();
    } else if (&treeWhichHasBeenChanged == &_treeManager.getAPVTS().state) {
        // if we get here, the plugin state has been loaded. we need to deal with setting internal params from those of the state.
        updateHistogramEqualization();
        updateStatistic();

        updateAllDimensionwiseFeatures();
        updateStatistic();
    }
}

using EventwiseStatisticsF = nvs::analysis::EventwiseStatistics<float>;


juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
                                    std::vector<float> const &normalizedOnsets,
                                    const juce::String& waveformHash,
                                    const juce::String& audioAbsPath);

void TimbreSpace::changeListenerCallback(juce::ChangeBroadcaster* source) {
    // could there be any reason to clear the tree? re-assigning it wouldn't need that, but
    // what if the rest of this func fails? do we want a cleared tree at that point?
    if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
        // TimbreSpace really only cares about getting both Onsets and TimbreSpaceAnalysis; it cannot complete its tasks without both.
        // ========================================ONSETS========================================
        const auto onsetsResult = a->shareOnsetAnalysis();
        if (!onsetsResult) {
            DBG("Onsets somehow null; returning\n");
            return;
        }
        auto const &onsets = onsetsResult->onsets;
        if (onsets.empty()) {
            DBG("Onsets have 0 length");
            return;
        }

        // ===================================TIMBRE ANALYSIS====================================
        const auto analysisResult = a->stealTimbreSpaceRepresentation();
        if (!analysisResult.has_value()){
            DBG("No analysis available\n");
            return;
        }
        auto const &tspace = analysisResult.value().timbreMeasurements;

        const String waveformHash = onsetsResult->waveformHash;
        const String absFilePath = onsetsResult->audioFileAbsPath;

        if (waveformHash != analysisResult.value().waveformHash || absFilePath != analysisResult.value().audioFileAbsPath) {
            DBG("Discrepancy between onsets and timbre analysis\n");
        }
        setTimbreSpaceTree(timbreSpaceReprToVT(tspace, onsets, waveformHash, absFilePath));

        setSavePending(true);
        signalSaveAnalysisOption();
        signalOnsetsAvailable();
    }
}
//=============================================================================================================================

void TimbreSpace::updateHistogramEqualization() {
    settings.histogramEqualization = *_treeManager.getAPVTS().getRawParameterValue(axiom::histogram_equalization);
}
void TimbreSpace::updateStatistic() {
    settings.statistic = static_cast<nvs::analysis::Statistic>(_treeManager.getAPVTS().getRawParameterValue(axiom::statistic)->load());
}


void addEventwiseStatistics(juce::ValueTree& tree, const analysis::EventwiseStatistics<analysis::Real>& stats) {
	tree.setProperty(axiom::mean, stats.mean, nullptr);
	tree.setProperty(axiom::median, stats.median, nullptr);
	tree.setProperty(axiom::variance, stats.variance, nullptr);
	tree.setProperty(axiom::skewness, stats.skewness, nullptr);
	tree.setProperty(axiom::kurtosis, stats.kurtosis, nullptr);
}
analysis::EventwiseStatistics<analysis::Real> toEventwiseStatistics(juce::ValueTree const &vt){
	return {
		.mean = vt.getProperty(axiom::mean),
		.median = vt.getProperty(axiom::median),
		.variance = vt.getProperty(axiom::variance),
		.skewness = vt.getProperty(axiom::skewness),
		.kurtosis = vt.getProperty(axiom::kurtosis)
	};
}
void TimbreSpace::setTimbreSpaceTree(ValueTree const &timbreSpaceTree) {
	_treeManager.setTimbreSpaceTree(timbreSpaceTree);
    signalTimbreSpaceTreeChanged();
    const auto onsetsVar = timbreSpaceTree.getProperty(axiom::NormalizedOnsets);
    if (const Array<var> *onsetsArray = onsetsVar.getArray()) {

        const juce::ValueTree mdTree = timbreSpaceTree.getChildWithName(axiom::Metadata);
        const auto waveformHash = mdTree.getProperty(axiom::audioHash).toString();
        if (const auto path = mdTree.getProperty(axiom::AudioFilePathAbsolute).toString();
            !(waveformHash.isEmpty() || path.isEmpty()))
        {
            std::vector<float> onsets(onsetsArray->begin(), onsetsArray->end());
            _onsetAnalysis = std::make_shared<analysis::OnsetAnalysisResult>(onsets, waveformHash, path);
        }
    }
	fullSelfUpdate(false);
}

juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
									std::vector<float> const &normalizedOnsets,
									const juce::String& waveformHash,
									const juce::String& audioAbsPath){
	ValueTree vt(axiom::TimbreAnalysis);
	{
		ValueTree md(axiom::Metadata);
		md.setProperty(axiom::Version, ProjectInfo::versionString, nullptr);
		md.setProperty(axiom::audioHash, waveformHash, nullptr);
		md.setProperty(axiom::AudioFilePathAbsolute, audioAbsPath, nullptr);
		md.setProperty(axiom::CreationTime, {}, nullptr);
		md.setProperty(axiom::AnalysisSettings, {}, nullptr);
		vt.addChild(md, 0, nullptr);
	}
	{
		var onsetArray;
		for (auto const &o : normalizedOnsets) {
			onsetArray.append(o);
		}
		vt.setProperty(axiom::NormalizedOnsets, onsetArray, nullptr);
	}
	{
		ValueTree timbreMeasurements("TimbreMeasurements");
		
		for (int frameIdx = 0; frameIdx < static_cast<int>(fullTimbreSpace.size()); ++frameIdx){
			const auto &timbreFrame = fullTimbreSpace[frameIdx];
			
			ValueTree frameTree(axiom::Frame);
			
			ValueTree bfccsTree(axiom::BFCCs);
		    {
			    const auto &bfccs = timbreFrame.bfccs();
		        for (int bfccIdx = 0; bfccIdx < static_cast<int>(bfccs.size()); ++bfccIdx){
		            ValueTree bfccTree("BFCC" + juce::String(bfccIdx));
		            addEventwiseStatistics(bfccTree, bfccs[bfccIdx]);
		            bfccsTree.addChild(bfccTree, bfccIdx, nullptr);
		        }
		    }
			frameTree.addChild(bfccsTree, -1, nullptr);
			
			// Add single-value features
		    for (auto feature : nvs::util::Iterator<analysis::Feature_e, static_cast<analysis::Feature_e>(analysis::NumBFCC), analysis::Feature_e::f0>()) {
		        ValueTree featureTree(analysis::toString(feature));
		        addEventwiseStatistics(featureTree, timbreFrame[feature]);
		        frameTree.addChild(featureTree, -1, nullptr);
		    }

			timbreMeasurements.addChild(frameTree, frameIdx, nullptr);
			
			vt.addChild(timbreMeasurements, 1, nullptr);
		}
	}

	return vt;
}
std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt)
{
    using namespace analysis;

	std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpace;
	
	auto timbreMeasurements = vt.getChildWithName(axiom::TimbreMeasurements);
	if (!timbreMeasurements.isValid())
		return timbreSpace;
	
	// Reserve space for efficiency
	timbreSpace.reserve(timbreMeasurements.getNumChildren());

    static_assert(static_cast<Feature_e>(0) == Feature_e::bfcc0); // we will be casting ints to Features for the BFCCs
    static_assert(static_cast<Feature_e>(12) == Feature_e::bfcc12);
	for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
	{
		auto frameTree = timbreMeasurements.getChild(frameIdx);
		FeatureContainer<EventwiseStatisticsF> frame;
		
		// Extract BFCCs
		if (auto bfccsTree = frameTree.getChildWithName(axiom::BFCCs);
		    bfccsTree.isValid())
		{
			for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
			{
				auto bfccTree = bfccsTree.getChild(bfccIdx);
				frame[static_cast<Feature_e>(bfccIdx)] = (toEventwiseStatistics(bfccTree));
			}
		}
		
		// Extract single-value features
	    for (auto const feature :  nvs::util::Iterator<Feature_e, static_cast<Feature_e>(NumBFCC), Feature_e::f0>()) {
	        if (auto featureTree = frameTree.getChildWithName(toString(feature));
                featureTree.isValid())
	        {
	            frame[feature] = toEventwiseStatistics(featureTree);
	        }
	    }

		timbreSpace.push_back(std::move(frame));
	}
	
	return timbreSpace;
}

std::vector<float> valueTreeToNormalizedOnsets(juce::ValueTree const &vt)
{
	std::vector<float> normalizedOnsets;

	const auto onsetArray = vt.getProperty(axiom::NormalizedOnsets);
	if (!onsetArray.isArray())
		return normalizedOnsets;
	
	auto* array = onsetArray.getArray();
	if (!array)
		return normalizedOnsets;
	
	normalizedOnsets.reserve(array->size());
	
	for (auto && e : *array)
	{
		normalizedOnsets.push_back(static_cast<float>(e));
	}
	
	return normalizedOnsets;
}

TimbreSpace::TreeManager::TreeManager(AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace)
: _apvts(apvts), _timbreSpace(timbreSpace) {
    _timbreSpaceTree.addListener(&_timbreSpace);
    _apvts.state.addListener(&_timbreSpace);
}
TimbreSpace::TreeManager::~TreeManager() {
    _timbreSpaceTree.removeListener(&_timbreSpace);
    _apvts.state.removeListener(&_timbreSpace);
}

juce::var TimbreSpace::TreeManager::getOnsetsVar() const {
	return _timbreSpaceTree.getProperty(axiom::NormalizedOnsets);
}
juce::ValueTree TimbreSpace::TreeManager::getTimbralFramesTree() const {
	return _timbreSpaceTree.getChildWithName("TimbreMeasurements");
}
const juce::ValueTree &TimbreSpace::TreeManager::getTimbreSpaceTree() const {
    return _timbreSpaceTree;
}
void TimbreSpace::TreeManager::setTimbreSpaceTree(ValueTree timbreSpaceTree) {
    _timbreSpaceTree = timbreSpaceTree;
}
int TimbreSpace::TreeManager::getNumFrames() const {
	const auto& onsets = _timbreSpaceTree.getProperty(axiom::NormalizedOnsets);
	jassert(onsets.isArray());
	int const numFrames = onsets.size();
#ifdef DBG
	auto const timbralFramesTree = getTimbralFramesTree();
    if(auto const numChildren = timbralFramesTree.getNumChildren();
        !numChildren == numFrames)
    {
	    DBG(util::valueTreeToXmlStringSafe(_timbreSpaceTree));
	    jassertfalse;
	}
#endif
	return numFrames;
}

void TimbreSpace::signalSaveAnalysisOption() const {
	sendActionMessage(axiom::saveAnalysis); // sends message, just to timbreSpaceComponent if one exists, to popup option to save analysis
}
void TimbreSpace::signalOnsetsAvailable() const {
	sendActionMessage(axiom::onsetsAvailable);	// who needs it? SegmentedWaveformComponent, TsnGranularPluginProcessor (for calling synth->loadOnsets())
}
void TimbreSpace::signalShapedPointsAvailable() const {
    sendActionMessage(axiom::shapedPointsAvailable);
}
void TimbreSpace::signalTimbreSpaceTreeChanged() const {
    sendActionMessage(axiom::timbreSpaceTreeChanged);   // for TimbreSpacePointSelector to know to update filtered feature
}

void TimbreSpace::TimbreDataManager::swapIfPending() {
    if (_pendingUpdate.exchange(false, std::memory_order_acq_rel))
    {
        _timbres5D = std::move(_timbres5D_pending);
    }
}

void TimbreSpace::fullSelfUpdate(const bool verbose){
	extractTimbralFeatures(verbose);
	computeHistogramEqualizedPoints(verbose);
	reshape(verbose);
	_timbreDataManager.setPendingReady();// _pendingUpdate.store(true, std::memory_order_release);
    _timbreDataManager.swapIfPending();

    signalShapedPointsAvailable();
}
// Internal template that works with any container
template<typename Container>
[[nodiscard]]
inline std::vector<analysis::Real>
extractFeaturesFromTreeImpl(const juce::ValueTree &frameTree,
                             const Container &featuresToUse,
                             const analysis::Statistic statisticToUse)
{
    using namespace analysis;
    std::vector<Real> out;

    if constexpr (requires { featuresToUse.size(); }) {
        out.reserve(featuresToUse.size());
    }

    juce::String statPropName;
    switch (statisticToUse) {
       case Statistic::Mean:     statPropName = axiom::mean;     break;
       case Statistic::Median:   statPropName = axiom::median;   break;
       case Statistic::Variance: statPropName = axiom::variance; break;
       case Statistic::Skewness: statPropName = axiom::skewness; break;
       case Statistic::Kurtosis: statPropName = axiom::kurtosis; break;
       default: jassertfalse;
    }

    for (auto f : featuresToUse) {
       const int idx = static_cast<int>(f);
       Real value = 0.0f;

       if (0 <= idx && idx < NumBFCC) {
          const auto bfccsTree = frameTree.getChildWithName(axiom::BFCCs);
          if (bfccsTree.isValid() && idx < bfccsTree.getNumChildren()) {
             auto bfccTree = bfccsTree.getChild(idx);
             value = bfccTree.getProperty(statPropName, 0.0f);
          }
       }
       else {
          juce::String childName;
          switch (f) {
            case Feature_e::SpectralCentroid:    childName = axiom::SpectralCentroid; break;
            case Feature_e::SpectralDecrease:    childName = axiom::SpectralDecrease; break;
            case Feature_e::SpectralFlatness:    childName = axiom::SpectralFlatness; break;
            case Feature_e::SpectralCrest:       childName = axiom::SpectralCrest; break;
            case Feature_e::SpectralComplexity:  childName = axiom::SpectralComplexity; break;
            case Feature_e::StrongPeak:          childName = axiom::StrongPeak;  break;
            case Feature_e::Periodicity:         childName = axiom::Periodicity; break;
            case Feature_e::Loudness:            childName = axiom::Loudness;    break;
            case Feature_e::f0:                  childName = axiom::f0;          break;
            default: jassertfalse;
          }

            if (auto scalarTree = frameTree.getChildWithName(childName);
                scalarTree.isValid())
            {
                value = scalarTree.getProperty(statPropName, 0.0f);
            }
       }

       out.push_back(value);
    }

    return out;
}

[[nodiscard]]
inline std::vector<analysis::Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        const std::vector<analysis::Feature_e> &featuresToUse,
                        const analysis::Statistic statisticToUse)
{
    return extractFeaturesFromTreeImpl(frameTree, featuresToUse, statisticToUse);
}

// Overload for single feature - wraps it in a std::array for iteration
[[nodiscard]]
inline std::vector<analysis::Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        const analysis::Feature_e featureToUse,
                        const analysis::Statistic statisticToUse)
{
    const std::array features{featureToUse};
    return extractFeaturesFromTreeImpl(frameTree, features, statisticToUse);
}

std::vector<float> TimbreSpace::getRawFeatureValues(const nvs::analysis::Feature_e feature) const {
    auto const s = nvs::analysis::toString(feature);

    if (nvs::util::isEmpty(_treeManager.getTimbreSpaceTree())){ return {}; }

    auto const &timbreTree = _treeManager.getTimbralFramesTree();

    std::vector<float> extractedFramewiseFeatureValues;

    for (int feat_idx = 0; feat_idx < timbreTree.getNumChildren(); ++feat_idx) {
        ValueTree const &frame = timbreTree.getChild(feat_idx);
        std::vector<float> v = extractFeaturesFromTree(frame, feature, settings.statistic);
        jassert (v.size() == 1);
        extractedFramewiseFeatureValues.push_back(v[0]);
    }
    return extractedFramewiseFeatureValues;
}

void TimbreSpace::extractTimbralFeatures(const bool verbose) {
    if (verbose)
        DBG("Extracting timbre points\n");

	auto const &featuresToExtract = settings.dimensionwiseFeatures;
	if (nvs::util::isEmpty(_treeManager.getTimbreSpaceTree())){
		if (verbose)
		    DBG("TimbreSpace::extractTimbralFeatures: timbre space empty, early exit\n");
		return;
	}
    _extractedFeatures.clearAll();
    _extractedFeatures.reserveAll(_treeManager.getNumFrames());
	
	auto const &timbralFramesTree = _treeManager.getTimbralFramesTree();
	for (int frameIdx = 0; frameIdx < timbralFramesTree.getNumChildren(); ++frameIdx) {
		ValueTree const &frame = timbralFramesTree.getChild(frameIdx);
		std::vector<float> v = extractFeaturesFromTree(frame, featuresToExtract, settings.statistic);
	    jassert(v.size() == 5);
	    for (size_t featIdx = 0; featIdx < v.size(); ++featIdx) {
	        _extractedFeatures.features[featIdx].push_back(v[featIdx]);
	    }
	}
    {
        // decorrelate!
        using namespace essentia;
        auto &features = _extractedFeatures.features;

        // compute means
        const auto x0_mean = mean(features[0]);
        const auto x1_mean = mean(features[1]);

        // center in place
        std::transform(features[0].begin(), features[0].end(), features[0].begin(),
                       [x0_mean](float x) { return x - x0_mean; });
        std::transform(features[1].begin(), features[1].end(), features[1].begin(),
                       [x1_mean](float x) { return x - x1_mean; });

        // compute covariance and variance on centered data (pass 0.0 as mean)
        const auto x0_x1_covar = covariance(features[0], 0.0f,
                                            features[1], 0.0f);
        const auto x0_var = variance(features[0], 0.0f);

        // decorrelate x1 from x0
        const float beta = x0_x1_covar / x0_var;
        std::transform(features[1].begin(), features[1].end(),
                       features[0].begin(),
                       features[1].begin(),
                       [beta](float x1, float x0) {
                           return x1 - beta * x0;
                       });
    }
}

std::vector<float> getHistoEqualizationVec(std::vector<float> const &points){
	std::vector<float> allX, vecOut;
	allX.reserve (points.size());
	vecOut.reserve (points.size());
	for (auto& p : points){
		allX.push_back (p);
	}
	std::ranges::sort (allX);
	
	for (auto const& p : points)
	{
		// find first index ≥ p.x
		const auto idx = static_cast<int>(std::ranges::lower_bound(allX,
                                                                   p)
                                          - allX.begin());
		const float quantile = static_cast<float>(idx) / static_cast<float>(allX.size() - 1);
		
		// optional: you can still gamma‑tweak the quantile
        constexpr float gamma = 0.8f; // <1 stretches mid‑values
		const float t = std::pow (quantile, gamma);
		vecOut.push_back(t);
	}
	return vecOut;
}

void TimbreSpace::computeHistogramEqualizedPoints(const bool verbose)
{	/** to be called when the actual analyzed timbre space changes  */

    if(verbose)
        DBG("updating timbre points\n");

	if (_extractedFeatures.allEmpty()){
		if (verbose)
		    DBG("updateAndDrawTimbreSpacePoints: raw features empty, returning...\n");
		return;
	}
	if (!_extractedFeatures.inValidState()) {
	    if (verbose)
	        DBG("updateAndDrawTimbreSpacePoints: raw features in invalid state, returning...\n");
	    return;
	}
	{
		auto const n_dim = _extractedFeatures.features.size();
		_ranges.clear();
		_ranges.reserve(n_dim);
		
		for (size_t i = 0; i < n_dim; ++i){
			_ranges.push_back(nvs::analysis::calculateRangeOfDimension(_extractedFeatures.features[i]));
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(_extractedFeatures.features[0].size());
		allDim1.reserve(_extractedFeatures.features[0].size());
		for (auto const& frame : _extractedFeatures.features[0]){
			allDim0.push_back(frame);	// e.g. bfcc1
		}
        for (auto const& frame : _extractedFeatures.features[1]){
            allDim1.push_back(frame);	// e.g. bfcc2
        }
        _histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		_histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
}
void TimbreSpace::reshape(const bool verbose)
{   /** to be called when we only want to change the VIEW of the timbre points (which will also need to happen when the timbre space itself changes) */
    if (verbose)
        DBG("reshaping timbre space\n");

    if (_extractedFeatures.allEmpty()){
        if (verbose)
            DBG("drawTimbreSpacePoints: raw features empty, returning...");
        return;
    }
    if (!_extractedFeatures.inValidState()) {
        if (verbose)
            DBG("drawTimbreSpacePoints: raw features in invalid state, returning...\n");
        return;
    }
    static constexpr size_t nDim {5};
    if ((_extractedFeatures.features.size() != _ranges.size()) || (nDim != _ranges.size())){
        if (verbose)
            DBG("drawTimbreSpacePoints: point size mismatch, exiting early");
        jassertfalse;
        return;
    }
    
    auto normalizer = [](const float x, const std::pair<float, float> &range) -> float
    {
        const auto r = (range.second - range.first);
        auto y01 = (x - range.first);
        if (r != 0){
            y01 /= r;
        }
        return juce::jmap(y01, -1.f, 1.f);
    };

    auto squash = [](const float xNorm) -> float { return std::asinh(10.0f*xNorm) / static_cast<float>(M_PI); };
    auto foo = [&](const float x, const std::pair<float, float> &range) -> float {
        return normalizer(x, range);
        // return squash(normalizer(x, range));
    };

    // clear points of timbreSpaceHolds
    clearPoints(); // clearing to make way for points we're about to be adding
    const auto numFrames = _extractedFeatures.features[0].size();

    std::vector<Timbre5DPoint> points;
    points.reserve(numFrames);
    const auto &features = _extractedFeatures.features;
    for (size_t i = 0; i < numFrames; ++i) {
        // ========================================2D========================================
        // squash normalized points within dimension range
        Timbre2DPoint pNL(foo(features[0][i], _ranges[0]),
                          foo(features[1][i], _ranges[1]));
        
        // histogram equalization
        float const &equalizedX = _histoEqualizedD0[i];
        float const &equalizedY = _histoEqualizedD1[i];
        
        Timbre2DPoint pHE(juce::jmap(equalizedX, -1.f, 1.f),
                          juce::jmap(equalizedY, -1.f, 1.f));
    
        float c = settings.histogramEqualization;
        jassert (0.0 <= c && c <= 1.0);
        Timbre2DPoint p = (1.f - c) * pNL + c * pHE; 
        
        // ========================================3D========================================
        Timbre3DPoint color(normalizer(features[2][i], _ranges[2]),
                           normalizer(features[3][i], _ranges[3]),
                           normalizer(features[4][i], _ranges[4]));
        
        // with this method, there is the guarantee that
        // the Nth member of timbreSpaceComponent._timbres5D corresponds to
        // the Nth member of onsets.
        constexpr float padding_scalar = 0.95f;

#pragma message("maybe we should build up the points, then add all instead of one by one. there is occassional bug when histo equalizing otherwise")
        points.emplace_back((Timbre5DPoint() << p * padding_scalar, color).finished());

        if (verbose){
            DBG(fmt::format("adding the point  {:.3f}, {:.3f}\n", p(0), p(1)));
        }
    }
    setPoints(points);
}


}	// namespace nvs::timbrespace
