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

namespace nvs::timbrespace {


//===============================================TimbreSpace=================================================

TimbreSpace::TimbreSpace(juce::AudioProcessorValueTreeState &apvts)
    :   _treeManager(apvts)
{
	_treeManager._timbreSpaceTree.addListener(this);
    _treeManager._apvts.state.addListener(this);
}
TimbreSpace::~TimbreSpace() {
	_treeManager._timbreSpaceTree.removeListener(this);
    _treeManager._apvts.state.removeListener(this);
}

void TimbreSpace::add5DPoint(const Timbre2DPoint &p2D, const Timbre3DPoint &p3D){
	using namespace nvs::util;
	assert(inRangeM1_1(p2D) && inRangeM1_1(p3D));
	_timbreDataManager.add5DPoint( to5D(p2D, p3D ));
}
void TimbreSpace::clearPoints() {
	_timbreDataManager.clear();
}

void TimbreSpace::TimbreDataManager::add5DPoint(const Timbre5DPoint &newPoint) {
    _timbres5D_pending.push_back(newPoint);
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

std::optional<std::vector<float>> TimbreSpace::getOnsets() const {
    if (const juce::var onsetsVar = _treeManager.getOnsetsVar();
        onsetsVar.isArray())
    {
		juce::Array<juce::var>* arr = onsetsVar.getArray();
		std::vector<float> out;
		out.reserve(arr->size());
		for (auto onset : *arr){
			out.push_back(onset);
		}
		return out;
	}
	return std::nullopt;
}
bool TimbreSpace::hasValidAnalysisFor(juce::String const &audioHash) const {
	return _audioFileHash.compare(audioHash) == 0;
}

static const std::map<juce::String, size_t> pidToDimensionMap {
    {"x_axis", 0},
    {"y_axis", 1},
    {"z_axis", 2},
    {"u_axis", 3},
    {"v_axis", 4},
};

void TimbreSpace::updateDimensionwiseFeature(const juce::String& paramID) {
    for (auto const &[s, i] : pidToDimensionMap) {
        if (paramID == s) {
            settings.dimensionwiseFeatures[i] = static_cast<nvs::analysis::Features>(_treeManager._apvts.getRawParameterValue(s)->load());
            fullSelfUpdate(true);
            return;
        }
    }
}
void TimbreSpace::updateAllDimensionwiseFeatures(){
    for (auto const &[s, i] : pidToDimensionMap) {
        const auto val = _treeManager._apvts.getRawParameterValue(s)->load();
        const auto feat = static_cast<nvs::analysis::Features>(val);
        settings.dimensionwiseFeatures[i] = feat;
    }
}
void TimbreSpace::updateStatistic() {
    settings.statistic = static_cast<nvs::analysis::Statistic>(_treeManager._apvts.getRawParameterValue("statistic")->load());
}
void TimbreSpace::valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier & property) {
    if (alteredTree.hasType("PARAM") && property == juce::Identifier("value")) {
        const auto paramID = alteredTree["id"].toString();
        const float newValue = alteredTree["value"];

        if (paramID == "histogram_equalization") {
            DBG("Histogram equalization changed to: " + juce::String(newValue));
            settings.histogramEqualization = *_treeManager._apvts.getRawParameterValue("histogram_equalization");
            std::cout << "tree changed! redrawing points...\n";

            fullSelfUpdate(true);
            return;
        }
        if (paramID == "statistic") {
            updateStatistic();
            std::cout << "tree changed! redrawing points...\n";
            fullSelfUpdate(true);
            return;
        }
        updateDimensionwiseFeature(paramID);
    }
}

void addEventwiseStatistics(juce::ValueTree& tree, const analysis::EventwiseStatistics<analysis::Real>& stats) {
	tree.setProperty("mean", stats.mean, nullptr);
	tree.setProperty("median", stats.median, nullptr);
	tree.setProperty("variance", stats.variance, nullptr);
	tree.setProperty("skewness", stats.skewness, nullptr);
	tree.setProperty("kurtosis", stats.kurtosis, nullptr);
}
analysis::EventwiseStatistics<analysis::Real> toEventwiseStatistics(juce::ValueTree const &vt){
	return {
		.mean = vt.getProperty("mean"),
		.median = vt.getProperty("median"),
		.variance = vt.getProperty("variance"),
		.skewness = vt.getProperty("skewness"),
		.kurtosis = vt.getProperty("kurtosis")
	};
}
void TimbreSpace::setTimbreSpaceTree(ValueTree const &timbreSpaceTree) {
	_treeManager._timbreSpaceTree = timbreSpaceTree;
	auto const mdTree = _treeManager._timbreSpaceTree.getChildWithName("Metadata");
	_audioFileHash = mdTree.getProperty("AudioFileHash", _audioFileHash).toString();
	_audioFileAbsPath = mdTree.getProperty("AudioFilePath (absolute)", _audioFileAbsPath).toString();

	fullSelfUpdate(true);
}
juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> const &fullTimbreSpace,
									std::vector<float> const &normalizedOnsets,
									const juce::String& audioFileHash,
									const juce::String& audioAbsPath){
	ValueTree vt("TimbreAnalysis");
	{
		ValueTree md("Metadata");
		md.setProperty("Version", ProjectInfo::versionString, nullptr);
		md.setProperty("AudioFileHash", audioFileHash, nullptr);
		md.setProperty("AudioFilePath (absolute)", audioAbsPath, nullptr);
		md.setProperty("CreationTime", {}, nullptr);
		md.setProperty("AnalysisSettings", {}, nullptr);
		vt.addChild(md, 0, nullptr);
	}
	{
		var onsetArray;
		for (auto const &o : normalizedOnsets) {
			onsetArray.append(o);
		}
		vt.setProperty("NormalizedOnsets", onsetArray, nullptr);
	}
	{
		ValueTree timbreMeasurements("TimbreMeasurements");
		
		for (int frameIdx = 0; frameIdx < static_cast<int>(fullTimbreSpace.size()); ++frameIdx){
			const auto &[bfccs, periodicity, loudness, f0] = fullTimbreSpace[frameIdx];
			
			ValueTree frameTree("Frame");
			
			ValueTree bfccsTree("BFCCs");
			for (int bfccIdx = 0; bfccIdx < static_cast<int>(bfccs.size()); ++bfccIdx){
				ValueTree bfccTree("BFCC" + juce::String(bfccIdx));
				addEventwiseStatistics(bfccTree, bfccs[bfccIdx]);
				bfccsTree.addChild(bfccTree, bfccIdx, nullptr);
			}
			frameTree.addChild(bfccsTree, -1, nullptr);
			
			// Add single-value features
			ValueTree periodicityTree("Periodicity");
			addEventwiseStatistics(periodicityTree, periodicity);
			frameTree.addChild(periodicityTree, -1, nullptr);
			
			ValueTree loudnessTree("Loudness");
			addEventwiseStatistics(loudnessTree, loudness);
			frameTree.addChild(loudnessTree, -1, nullptr);
			
			ValueTree f0Tree("F0");
			addEventwiseStatistics(f0Tree, f0);
			frameTree.addChild(f0Tree, -1, nullptr);
			
			timbreMeasurements.addChild(frameTree, frameIdx, nullptr);
			
			vt.addChild(timbreMeasurements, 1, nullptr);
		}
	}
	
	return vt;
}
std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt)
{
	std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> timbreSpace;
	
	auto timbreMeasurements = vt.getChildWithName("TimbreMeasurements");
	if (!timbreMeasurements.isValid())
		return timbreSpace;
	
	// Reserve space for efficiency
	timbreSpace.reserve(timbreMeasurements.getNumChildren());
	
	for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
	{
		auto frameTree = timbreMeasurements.getChild(frameIdx);
		nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF> frame;
		
		// Extract BFCCs
		if (auto bfccsTree = frameTree.getChildWithName("BFCCs");
		    bfccsTree.isValid())
		{
			frame.bfccs.reserve(bfccsTree.getNumChildren());
			
			for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
			{
				auto bfccTree = bfccsTree.getChild(bfccIdx);
				frame.bfccs.push_back(toEventwiseStatistics(bfccTree));
			}
		}
		
		// Extract single-value features
        if (auto periodicityTree = frameTree.getChildWithName("Periodicity");
            periodicityTree.isValid())
        {
            frame.periodicity = toEventwiseStatistics(periodicityTree);
        }
        if (auto loudnessTree = frameTree.getChildWithName("Loudness");
            loudnessTree.isValid())
        {
            frame.loudness = toEventwiseStatistics(loudnessTree);
        }
        if (auto f0Tree = frameTree.getChildWithName("F0");
            f0Tree.isValid())
        {
            frame.f0 = toEventwiseStatistics(f0Tree);
        }
		
		timbreSpace.push_back(std::move(frame));
	}
	
	return timbreSpace;
}

std::vector<float> valueTreeToNormalizedOnsets(juce::ValueTree const &vt)
{
	std::vector<float> normalizedOnsets;

	const auto onsetArray = vt.getProperty("NormalizedOnsets");
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

juce::var TimbreSpace::TreeManager::getOnsetsVar() const {
	return _timbreSpaceTree.getProperty("NormalizedOnsets");
}
juce::ValueTree TimbreSpace::TreeManager::getTimbralFramesTree() const {
	return _timbreSpaceTree.getChildWithName("TimbreMeasurements");
}
int TimbreSpace::TreeManager::getNumFrames() const {
	const auto& onsets = _timbreSpaceTree.getProperty("NormalizedOnsets");
	jassert(onsets.isArray());
	int const numFrames = onsets.size();
#ifdef DBG
	auto const timbralFramesTree = getTimbralFramesTree();
	auto const numChildren = timbralFramesTree.getNumChildren();
	jassert(numChildren == numFrames);
#endif
	return numFrames;
}

void TimbreSpace::signalSaveAnalysisOption() const {
	sendActionMessage("saveAnalysis"); // sends message, just to timbreSpaceComponent if one exists, to popup option to save analysis
}
void TimbreSpace::signalTimbreSpaceUpdated() const {
	sendActionMessage("reportAvailability");	// who needs it? TsnGranularPluginEditor and TsnGranularAudioProcessor
}
void TimbreSpace::valueTreeRedirected (ValueTree &treeWhichHasBeenChanged) {
	if (&treeWhichHasBeenChanged == &_treeManager._timbreSpaceTree){
		signalTimbreSpaceUpdated();
	} else if (&treeWhichHasBeenChanged == &_treeManager._apvts.state) {
        // if we get here, the plugin state has been loaded. we need to deal with setting dimensionwise features from state.
        updateAllDimensionwiseFeatures();
        updateStatistic();
    }
}

void TimbreSpace::changeListenerCallback(juce::ChangeBroadcaster* source) {
	// could there be any reason to clear the tree? re-assigning it wouldn't need that, but
	// what if the rest of this func fails? do we want a cleared tree at that point?
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		const auto analysisResult = a->stealTimbreSpaceRepresentation();
		if (!analysisResult.has_value()){
			fmt::print("No analysis available");
			return;
		}
		auto const &onsets = analysisResult.value().onsets;
		auto const &tspace = analysisResult.value().timbreMeasurements;
		auto const audioHash = analysisResult.value().audioHash;
		_audioFileAbsPath = analysisResult.value().audioFileAbsPath;
		if (onsets.empty()) {
			fmt::print("Onsets have 0 length");
			return;
		}
		setTimbreSpaceTree(timbreSpaceReprToVT(tspace, onsets, audioHash, _audioFileAbsPath));

	    setSavePending(true);
        signalSaveAnalysisOption();
		signalTimbreSpaceUpdated();
	}
}
void TimbreSpace::TimbreDataManager::updateData(const bool verbose) {
    if (verbose)
        DBG("TimbreDataManager::updateData computing _delaunator & storing _pendingUpdate flag\n");
    if (_timbres5D_pending.empty()) {
        return;
    }
    _delaunator_pending = std::make_unique<delaunator::Delaunator>(make2dCoordinates(_timbres5D_pending));
    _pendingUpdate.store(true, std::memory_order_release);
}
void TimbreSpace::TimbreDataManager::swapIfPending(const bool verbose) {
    if (_pendingUpdate.exchange(false, std::memory_order_acq_rel)) {
        if (verbose)
            DBG("TimbreDataManager::swapIfPending exchanging state\n");
        _timbres5D = std::move(_timbres5D_pending);
        _delaunator = std::move(_delaunator_pending);
    }
}

void TimbreSpace::fullSelfUpdate(const bool verbose){
	extract(verbose);
	updateTimbreSpacePoints(verbose);
	reshape(verbose);
	_timbreDataManager.updateData(verbose);
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
		case Statistic::Mean:     statPropName = "mean";     break;
		case Statistic::Median:   statPropName = "median";   break;
		case Statistic::Variance: statPropName = "variance"; break;
		case Statistic::Skewness: statPropName = "skewness"; break;
		case Statistic::Kurtosis: statPropName = "kurtosis"; break;
		default: jassertfalse;
	}
	
	for (auto f : featuresToUse) {
		const int idx = static_cast<int>(f);
		Real value = 0.0f;
		
		if (0 <= idx && idx < NumBFCC) {
			// It's a BFCC - get from BFCCs array
			const auto bfccsTree = frameTree.getChildWithName("BFCCs");
			if (bfccsTree.isValid() && idx < bfccsTree.getNumChildren()) {
				auto bfccTree = bfccsTree.getChild(idx);
				value = bfccTree.getProperty(statPropName, 0.0f);
			}
		}
		else {
			// It's one of the scalars
			juce::String childName;
			switch (f) {
				case Features::Periodicity: childName = "Periodicity"; break;
				case Features::Loudness:    childName = "Loudness";    break;
				case Features::f0:          childName = "F0";          break;
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
	if (nvs::util::isEmpty(_treeManager._timbreSpaceTree)){
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

void TimbreSpace::updateTimbreSpacePoints(const bool verbose)
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
        add5DPoint(p * padding_scalar, color);
        
        if (verbose){
            DBG(fmt::format("adding the point  {:.3f}, {:.3f}\n", p(0), p(1)));
        }
    }
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
        jassert (w > 0.0);
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
