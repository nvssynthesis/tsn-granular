/*
  ==============================================================================

    TimbreSpace.cpp
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpace.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "../Analysis/ThreadedAnalyzer.h"
#include "TimbreSpaceTriangulation.h"
#include <random>
#include <ranges>
#include "fmt/core.h"
#include "StringAxiom.h"

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

std::vector<Timbre5DPoint> const &TimbreSpace::getTimbreSpace() const {
    return _timbreDataManager.getTimbres();
}

const std::vector<Timbre5DPoint> &TimbreSpace::TimbreDataManager::getTimbres() const {
    return _timbres5D;
}

std::vector<util::WeightedIdx> const &TimbreSpace::getCurrentPointIndices() const {
    return _currentPointIndices;
}

std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);

Timbre5DPoint TimbreSpace::getTargetPoint() const {
    return _target;
}

void TimbreSpace::setTargetPoint(const Timbre5DPoint& target) {
    _target = target;
}
bool TimbreSpace::TimbreDataManager::isReadyForTriangulation() const {
    if (isEmpty())
        return false;
    if (_delaunator.get() == nullptr)
        return false;

    return true;
}

// Modified main function with method parameter
void TimbreSpace::computeExistingPointsFromTarget()
{
    _timbreDataManager.swapIfPending(true);
    if (!_timbreDataManager.isReadyForTriangulation())
        return;

	std::vector<util::WeightedIdx> const weightedIndices =
	    findPointsTriangulationBased(
	        _target,
	        _timbreDataManager.getTimbres(),
	        *_timbreDataManager.getDelaunator()
	    );
	jassert(weightedIndices.size() == 3);
	
	for (auto const &widx : weightedIndices) {
		jassert(0 <= widx.idx);
		jassert((widx.idx < _timbreDataManager.size()));
	}
	
	_currentPointIndices = weightedIndices;
}
String TimbreSpace::getAudioAbsolutePath() const {
    if (_onsetAnalysis == nullptr) {
        return "";
    }
    return _onsetAnalysis->audioFileAbsPath;
}
auto TimbreSpace::shareOnsets() const -> std::shared_ptr<analysis::ThreadedAnalyzer::OnsetAnalysisResult>  {
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

void TimbreSpace::updateDimensionwiseFeature(const juce::String& paramID) {
    for (auto const &[s, i] : pidToDimensionMap) {
        if (paramID == s) {
            settings.dimensionwiseFeatures[i] = static_cast<nvs::analysis::Features>(_treeManager.getAPVTS().getRawParameterValue(s)->load());
            fullSelfUpdate(true);
            return;
        }
    }
}
void TimbreSpace::updateAllDimensionwiseFeatures(){
    for (auto const &[s, i] : pidToDimensionMap) {
        const auto val = _treeManager.getAPVTS().getRawParameterValue(s)->load();
        const auto feat = static_cast<nvs::analysis::Features>(val);
        settings.dimensionwiseFeatures[i] = feat;
    }
}
void TimbreSpace::updateStatistic() {
    settings.statistic = static_cast<nvs::analysis::Statistic>(_treeManager.getAPVTS().getRawParameterValue(axiom::statistic)->load());
}
void TimbreSpace::valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier & property) {
    if (alteredTree.hasType(nvs::axiom::PARAM) && property == juce::Identifier("value")) {
        const auto paramID = alteredTree["id"].toString();
        const float newValue = alteredTree["value"];

        if (paramID == nvs::axiom::histogram_equalization) {
            DBG("Histogram equalization changed to: " + juce::String(newValue));
            settings.histogramEqualization = *_treeManager.getAPVTS().getRawParameterValue(axiom::histogram_equalization);
            std::cout << "tree changed! redrawing points...\n";

            fullSelfUpdate(true);
            return;
        }
        if (paramID == nvs::axiom::statistic) {
            updateStatistic();
            std::cout << "tree changed! redrawing points...\n";
            fullSelfUpdate(true);
            return;
        }
        updateDimensionwiseFeature(paramID);
    }
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
    const auto onsetsVar = timbreSpaceTree.getProperty(axiom::NormalizedOnsets);
    if (const Array<var> *onsetsArray = onsetsVar.getArray()) {

        const juce::ValueTree mdTree = timbreSpaceTree.getChildWithName(axiom::Metadata);
        const auto waveformHash = mdTree.getProperty(axiom::audioHash).toString();
        if (const auto path = mdTree.getProperty(axiom::AudioFilePathAbsolute).toString();
            !(waveformHash.isEmpty() || path.isEmpty()))
        {
            std::vector<float> onsets(onsetsArray->begin(), onsetsArray->end());
            _onsetAnalysis = std::make_shared<analysis::ThreadedAnalyzer::OnsetAnalysisResult>(onsets, waveformHash, path);
        }
    }
	fullSelfUpdate(true);
}
juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> const &fullTimbreSpace,
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
		    for (auto feature : nvs::util::Iterator<analysis::Features, static_cast<analysis::Features>(analysis::NumBFCC), analysis::Features::f0>()) {
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
std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt)
{
    using namespace analysis;

	std::vector<FeatureContainer<TimbreSpace::EventwiseStatisticsF>> timbreSpace;
	
	auto timbreMeasurements = vt.getChildWithName(axiom::TimbreMeasurements);
	if (!timbreMeasurements.isValid())
		return timbreSpace;
	
	// Reserve space for efficiency
	timbreSpace.reserve(timbreMeasurements.getNumChildren());

    static_assert(static_cast<Features>(0) == Features::bfcc0); // we will be casting ints to Features for the BFCCs
    static_assert(static_cast<Features>(12) == Features::bfcc12);
	for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
	{
		auto frameTree = timbreMeasurements.getChild(frameIdx);
		FeatureContainer<TimbreSpace::EventwiseStatisticsF> frame;
		
		// Extract BFCCs
		if (auto bfccsTree = frameTree.getChildWithName(axiom::BFCCs);
		    bfccsTree.isValid())
		{
			for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
			{
				auto bfccTree = bfccsTree.getChild(bfccIdx);
				frame[static_cast<Features>(bfccIdx)] = (toEventwiseStatistics(bfccTree));
			}
		}
		
		// Extract single-value features
	    for (auto const feature :  nvs::util::Iterator<Features, static_cast<Features>(NumBFCC), Features::f0>()) {
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
void TimbreSpace::valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) {
	if (&treeWhichHasBeenChanged == &_treeManager.getTimbreSpaceTree()){
		signalOnsetsAvailable();
	} else if (&treeWhichHasBeenChanged == &_treeManager.getAPVTS().state) {
        // if we get here, the plugin state has been loaded. we need to deal with setting dimensionwise features from state.
        updateAllDimensionwiseFeatures();
        updateStatistic();
    }
}

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
void TimbreSpace::TimbreDataManager::triangulatePendingData(const bool verbose) {
    if (verbose)
        DBG("TimbreDataManager::triangulatePendingData computing _delaunator & storing _pendingUpdate flag\n");
    if (_timbres5D_pending.empty()) {
        if (verbose)
            DBG("TimbreDataManager::triangulatePendingData timbres5D empty; returning\n");
        return;
    }
    const auto coords2D = make2dCoordinates(_timbres5D_pending);
    try {
        _delaunator_pending = std::make_unique<delaunator::Delaunator>(coords2D);   // IF THIS FAILS, _pendingUpdate does not store `true`
    }
    catch (std::exception &e) {
        DBG(e.what());
        return;
    }
    catch (...) {
        return;
    }
    _pendingUpdate.store(true, std::memory_order_release);
}
void TimbreSpace::TimbreDataManager::swapIfPending(const bool verbose) {
    if (_pendingUpdate.exchange(false, std::memory_order_acq_rel))
    {
        if (verbose) {
            DBG("TimbreDataManager::swapIfPending exchanging state\n");
        }
        _timbres5D = std::move(_timbres5D_pending);
        _delaunator = std::move(_delaunator_pending);
    }
}

void TimbreSpace::fullSelfUpdate(const bool verbose){
	extract(verbose);
	computeHistogramEqualizedPoints(verbose);
	reshape(verbose);
	_timbreDataManager.triangulatePendingData(verbose);
}

[[nodiscard]]
inline std::vector<analysis::Real>
extractFeatures(const juce::ValueTree &frameTree,
                const std::vector<analysis::Features> &featuresToUse,
                const analysis::Statistic statisticToUse)
{
	using namespace analysis;
	std::vector<Real> out;
	out.reserve(featuresToUse.size());
	
	// Get the statistic property name
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
			// It's a BFCC - get from BFCCs array
			const auto bfccsTree = frameTree.getChildWithName(axiom::BFCCs);
			if (bfccsTree.isValid() && idx < bfccsTree.getNumChildren()) {
				auto bfccTree = bfccsTree.getChild(idx);
				value = bfccTree.getProperty(statPropName, 0.0f);
			}
		}
		else {
			// It's one of the scalars
			juce::String childName;
			switch (f) {
			    case Features::SpectralCentroid:    childName = axiom::SpectralCentroid; break;
			    case Features::SpectralDecrease:    childName = axiom::SpectralDecrease; break;
			    case Features::SpectralFlatness:    childName = axiom::SpectralFlatness; break;
			    case Features::SpectralCrest:       childName = axiom::SpectralCrest; break;
			    case Features::SpectralComplexity:  childName = axiom::SpectralComplexity; break;
			    case Features::StrongPeak:          childName = axiom::StrongPeak; break;
				case Features::Periodicity:         childName = axiom::Periodicity; break;
				case Features::Loudness:            childName = axiom::Loudness;    break;
				case Features::f0:                  childName = axiom::F0;          break;
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

void TimbreSpace::extract(const bool verbose) {
    if (verbose)
        DBG("Extracting timbre points\n");

	auto const &featuresToExtract = settings.dimensionwiseFeatures;
	if (nvs::util::isEmpty(_treeManager.getTimbreSpaceTree())){
		if (verbose)
		    DBG("TimbreSpace::extract: timbre space empty, early exit\n");
		return;
	}
	_eventwiseExtractedTimbrePoints.clear();
	_eventwiseExtractedTimbrePoints.reserve(_treeManager.getNumFrames());
	
	auto const &timbreTree = _treeManager.getTimbralFramesTree();
	for (int i = 0; i < timbreTree.getNumChildren(); ++i) {
		ValueTree const &frame = timbreTree.getChild(i);
		std::vector<float> v = extractFeatures(frame, featuresToExtract, settings.statistic);
		_eventwiseExtractedTimbrePoints.push_back(v);
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

	if (_eventwiseExtractedTimbrePoints.empty()){
		if (verbose)
		    DBG("updateAndDrawTimbreSpacePoints: timbreSpace empty, returning...\n");
		return;
	}
	{
		auto const n_dim = _eventwiseExtractedTimbrePoints[0].size();
		_ranges.clear();
		_ranges.reserve(n_dim);
		
		for (size_t i = 0; i < n_dim; ++i){
			_ranges.push_back(nvs::analysis::calculateRangeOfDimension(_eventwiseExtractedTimbrePoints, i));
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(_eventwiseExtractedTimbrePoints.size());
		allDim1.reserve(_eventwiseExtractedTimbrePoints.size());
		for (auto const& frame : _eventwiseExtractedTimbrePoints){
			allDim0.push_back(frame[0]);	// e.g. bfcc1
			allDim1.push_back(frame[1]);	// e.g. bfcc2
		}
		_histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		_histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
}
void TimbreSpace::reshape(const bool verbose)
{   /** to be called when we only want to change the VIEW of the timbre points (which will also need to happen when the timbre space itself changes) */
    if (verbose)
        DBG("reshaping timbre space\n");

    if (_eventwiseExtractedTimbrePoints.empty()){
        if (verbose)
            DBG("drawTimbreSpacePoints: _eventwiseExtractedTimbrePoints empty, returning...");
        return;
    }
    
    std::vector<std::vector<float>> const &timbreSpaceRepr = _eventwiseExtractedTimbrePoints;
    if (timbreSpaceRepr[0].size() != _ranges.size()){
        if (verbose)
            DBG("drawTimbreSpacePoints: point size mismatch, exiting early");
        jassertfalse;
        return;
    }
    
    auto normalizer = [](float x, std::pair<float, float> range) -> float
    {
        auto r = (range.second - range.first);
        auto y01 = (x - range.first);
        if (r != 0){
            y01 /= r;
        }
        return juce::jmap(y01, -1.f, 1.f);
    };
    
    auto squash = [](const float xNorm) -> float { return std::asinh(10.0f*xNorm) / static_cast<float>(M_PI); };
    auto foo = [&](const float x, const std::pair<float, float> &range) -> float { return squash(normalizer(x, range)); };

    // clear points of timbreSpaceHolds
    clearPoints(); // clearing to make way for points we're about to be adding

    std::vector<Timbre5DPoint> points;
    points.reserve(timbreSpaceRepr.size());
    for (size_t i = 0; i < timbreSpaceRepr.size(); ++i) {
        static constexpr size_t nDim {5};
        std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
        
        jassert (timbreFrame.size() >= nDim);

        // ========================================2D========================================
        // squash normalized points within dimension range
        Timbre2DPoint pNL(foo(timbreFrame[0], _ranges[0]),
                          foo(timbreFrame[1], _ranges[1]));
        
        // histogram equalization
        float const &equalizedX = _histoEqualizedD0[i];
        float const &equalizedY = _histoEqualizedD1[i];
        
        Timbre2DPoint pHE(juce::jmap(equalizedX, -1.f, 1.f),
                          juce::jmap(equalizedY, -1.f, 1.f));
    
        float c = settings.histogramEqualization;
        jassert (0.0 <= c && c <= 1.0);
        Timbre2DPoint p = (1.f - c) * pNL + c * pHE; 
        
        // ========================================3D========================================
        Timbre3DPoint color(normalizer(timbreFrame[2], _ranges[2]),
                           normalizer(timbreFrame[3], _ranges[3]),
                           normalizer(timbreFrame[4], _ranges[4]));
        
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

/**
 * Finds the K nearest points to `target` in `database`,
 * builds softmax-style weights over those K,
 * then either returns them all, or samples `numToPick` **without replacement**.
 *
 * @param  target          the target point from which to find nearest points
 * @param  database        your full set of Timbre5DPoints
 * @param  K               how many nearest neighbors to consider (caps at database.size())
 * @param  numToPick       if <=0, we return all K weighted indices;
 *                         otherwise we pick exactly numToPick **without** replacement.
 * @param  sharpness       0=flat, >0 biases toward closer points
 * @param  higher3Dweight  extra weighting on the 3D portion of your distance metric
 */
std::vector<util::WeightedIdx> findPointsDistanceBased(
    const Timbre5DPoint&               target,
    const juce::Array<Timbre5DPoint>&  database,
    int                                K,
    const int                          numToPick,
    double                             sharpness,
    float                              higher3Dweight)
{
    if (database.isEmpty()) { return {}; }

    std::vector<util::WeightedIdx> weightedIndices = [&database, target, higher3Dweight, &K, sharpness](){
       // compute squared distances
       std::vector<util::DistanceIdx> distIdx;
       distIdx.reserve (database.size());
       for (int i = 0; i < database.size(); ++i) {
          auto const& p = database.getReference(i);

          // Extract 2D portions and compute difference
          Timbre2DPoint diff2D = get2D(p) - get2D(target);

          // Extract 3D portions and compute difference
          Timbre3DPoint diff3D = get3D(p) - get3D(target);

          // Compute squared distance
          double d2 = diff2D.squaredNorm() + higher3Dweight * diff3D.squaredNorm();

          distIdx.emplace_back (i, d2);
       }

       // keep only the K nearest
       {
          K = std::min<int> (K, (int) distIdx.size());
          std::ranges::nth_element (distIdx, distIdx.begin() + K
                                    ,
                                    [](auto& a, auto& b){ return a.distance < b.distance; });
          distIdx.resize (K);
          assert(distIdx.size() == static_cast<size_t>(K));
       }
       return toWeightedIndices(distIdx, sharpness);
    }();

    /** if caller just wants the full distribution, return it: */
    if (numToPick <= 0 || numToPick >= K) { return weightedIndices; }

    /** otherwise sample numToPick *without replacement* by repeatedly drawing
     from the discrete distribution then zeroing out that weight and re-normalizing. */
    std::vector<util::WeightedIdx> picked;
    picked.reserve (numToPick);

    // extract weights into their own vector using views
    auto weightView = weightedIndices | std::views::transform([](const auto& wi) { return wi.weight; });
    std::vector<double> weightArr(weightView.begin(), weightView.end());

    std::mt19937 rng { std::random_device{}() };
    for (int pick = 0; pick < numToPick; ++pick) {
       // construct the discrete_distribution over the weights
       std::discrete_distribution<int> dist(weightArr.begin(), weightArr.end());
       int choice = dist(rng);
       picked.push_back(weightedIndices[choice]);
       // zero out that weight so it's never picked again
       weightArr[choice] = 0.0;

       // Check if any weights remain (optional optimization)
       if (std::ranges::all_of(weightArr, [](const double w) { return w == 0.0; })) {
          break;
       }
    }
    return picked;
}
std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower)
{
	if (dv.empty()) {
		return {};
	}
	
	// 1) Find maximum distance for normalization
	double dmax = 0.0;
	for (const auto& di : dv) {
		dmax = std::max(dmax, di.distance);
	}
	
	// 2) Convert distances to initial weights using softmax
	std::vector<double> weights;
	weights.reserve(dv.size());
	
	if (dmax <= 0.0) {
		// All distances are zero - uniform weights
		const double uniformWeight = 1.0 / static_cast<double>(dv.size());
		weights.assign(dv.size(), uniformWeight);
	} else {
		// Apply softmax: exp(-sharpness * (distance/dmax))
		for (const auto& di : dv) {
			const double normalizedDist = di.distance / dmax;
			weights.push_back(std::exp(-sharpness * normalizedDist));
		}
		
		// Normalize to sum = 1
		const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		for (auto& w : weights) {
			w /= sum;
		}
	}
	
	// 3) Apply contrast power scaling
	if (contrastPower != 1.0) {
		for (auto& w : weights) {
			w = std::pow(w, contrastPower);
		}
		
		// Re-normalize after power scaling
        if (const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
            sum > 0.0)
        {
			for (auto& w : weights) {
				w /= sum;
			}
		}
	}
	
	// 4) Build result vector with WeightedIdx structs
	std::vector<util::WeightedIdx> result;
	result.reserve(dv.size());
	
	for (size_t i = 0; i < dv.size(); ++i) {
		result.emplace_back(dv[i].idx, weights[i]);
	}
	
	return result;
}

// triangulation-based point selection function
std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target, const std::vector<Timbre5DPoint>& database, const delaunator::Delaunator &d)
{
	if (database.empty()) { return {}; }
    const auto dbSizeAtStart = database.size();

	// Need at least 3 points for triangulation
	jassert (database.size() >= 3);
	
	// Find triangle containing the target point
	const Timbre2DPoint targetPoint = get2D(target);
	const auto triangleOpt = findContainingTriangle(d, targetPoint);
	
	if (!triangleOpt.has_value()) {
		// Target point is outside the convex hull
		// Fall back to distance-based method or handle as edge case
		const auto result = findNearestTrianglePoints(target, database, d);
	    jassert (result.size() == 3);
	    return result;
	}
	
	// Get the triangle vertices
	const auto triangle = triangleOpt.value();
	const size_t idx0 = triangle[0];
	const size_t idx1 = triangle[1];
	const size_t idx2 = triangle[2];
	{
	    const auto dbSize = database.size();
	    jassert (dbSize == dbSizeAtStart);
	    jassert (idx0 < dbSize);
	    jassert (idx1 < dbSize);
	    jassert (idx2 < dbSize);
	}

	// Get the 2D points for barycentric calculation
	const Timbre2DPoint p0 = get2D(database[idx0]);
	const Timbre2DPoint p1 = get2D(database[idx1]);
	const Timbre2DPoint p2 = get2D(database[idx2]);
	
	// Compute barycentric weights
	const auto weights = computeBarycentricWeights(targetPoint, p0, p1, p2);
    for (auto & w : weights) {
        jassert (w >= 0.0);
    }
	
	// Create result vector
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(idx0, weights[0]);
	result.emplace_back(idx1, weights[1]);
	result.emplace_back(idx2, weights[2]);

    jassert (result.size() == 3);
	return result;
}

// Helper function for when target is outside convex hull
std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const std::vector<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d)
{
	// Find the nearest edge or vertex of the convex hull
	// This is a simplified approach - you might want to be more sophisticated
	
	Timbre2DPoint targetPoint = get2D(target);
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle {0, 1, 2};
	
	// Check all triangles and find the one with minimum distance to target
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		const size_t idx0 = d.triangles[i];
		const size_t idx1 = d.triangles[i + 1];
		const size_t idx2 = d.triangles[i + 2];
		
		Timbre2DPoint p0 = get2D(database[idx0]);
		Timbre2DPoint p1 = get2D(database[idx1]);
		Timbre2DPoint p2 = get2D(database[idx2]);
		
		// Calculate centroid of triangle
		Timbre2DPoint centroid = (p0 + p1 + p2) / 3.0;

        if (const double distance = (targetPoint - centroid).squaredNorm();
            distance < minDistance)
        {
			minDistance = distance;
			bestTriangle = {idx0, idx1, idx2};
		}
	}
	
	// Use the closest triangle and project the point onto it
	// For simplicity, we'll use equal weights, but you could do proper projection
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(bestTriangle[0], 1.0/3.0);
	result.emplace_back(bestTriangle[1], 1.0/3.0);
	result.emplace_back(bestTriangle[2], 1.0/3.0);
	
	return result;
}

}	// namespace nvs::timbrespace
