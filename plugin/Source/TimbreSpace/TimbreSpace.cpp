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

TimbreSpace::TimbreSpace() {
	treeManager.tree.addListener(this);
}
TimbreSpace::~TimbreSpace() {
	treeManager.tree.removeListener(this);
}

void TimbreSpace::add5DPoint(Timbre2DPoint p2D, Timbre3DPoint p3D){
	using namespace nvs::util;
	assert(inRangeM1_1(p2D) && inRangeM1_1(p3D));
	timbres5D.add( to5D(p2D, p3D ));
}

void TimbreSpace::clear() {
	timbres5D.clear();
}


std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);

Timbre5DPoint TimbreSpace::getTargetPoint() const {
    return _target;
}

void TimbreSpace::setTargetPoint(const Timbre5DPoint& target) {
    _target = target;
}
// Modified main function with method parameter
void TimbreSpace::computeExistingPointsFromTarget(int K_neighbors,
												  double sharpness,
												  float higher3Dweight,
												  PointSelectionMethod method)
{
	if (timbres5D.size() == 0) { return; }
	
	std::vector<util::WeightedIdx> const weightedIndices = [this, K_neighbors, sharpness, higher3Dweight, method]()
	{
		switch (method) {
			case PointSelectionMethod::TRIANGULATION_BASED:
				return findPointsTriangulationBased(_target, timbres5D, *_delaunator);
				break;
			case PointSelectionMethod::DISTANCE_BASED:
				return findPointsDistanceBased(_target, timbres5D, K_neighbors, 3, sharpness, higher3Dweight);
				break;
		}
	}();
	jassert(weightedIndices.size() == 3);
	
	for (auto const &widx : weightedIndices) {
		jassert(0 <= widx.idx);
		jassert((widx.idx < timbres5D.size()) || ((timbres5D.size() == 0) && (widx.idx == 0)));
	}
	
	currentPointIndices = weightedIndices;
}

std::optional<std::vector<float>> TimbreSpace::getOnsets() const {
    if (const juce::var onsetsVar = treeManager.getOnsetsVar();
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

void TimbreSpace::valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier &) {
	if (alteredTree.hasType("TimbreSpace")){
		std::cout << "tree changed! redrawing points...\n";
		settings.histogramEqualization = static_cast<double>(alteredTree.getProperty("HistogramEqualization"));
		auto xs = static_cast<juce::String>(alteredTree.getProperty("xAxis"));
		auto ys = static_cast<juce::String>(alteredTree.getProperty("yAxis"));
		std::cout << "x: " << xs << " y: " << ys << '\n';
		settings.dimensionWisefeatures[0] = nvs::analysis::toFeature(xs);
		settings.dimensionWisefeatures[1] = nvs::analysis::toFeature(ys);
		fullSelfUpdate(true);
	}
	else {}
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
void TimbreSpace::setTimbreSpaceTree(ValueTree const &tree) {
	treeManager.tree = tree;
	auto const mdTree = tree.getChildWithName("Metadata");
	_audioFileHash = mdTree.getProperty("AudioFileHash", _audioFileHash).toString();
	_audioFileAbsPath = mdTree.getProperty("AudioFilePath (absolute)", _audioFileAbsPath).toString();

	fullSelfUpdate(true);
}
juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<TimbreSpace::EventwiseStatisticsF>> const &fullTimbreSpace,
									std::vector<float> const &normalizedOnsets,
									juce::String audioFileHash,
									juce::String audioAbsPath){
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
			const auto &frame = fullTimbreSpace[frameIdx];
			
			ValueTree frameTree("Frame");
			
			ValueTree bfccsTree("BFCCs");
			for (int bfccIdx = 0; bfccIdx < static_cast<int>(frame.bfccs.size()); ++bfccIdx){
				ValueTree bfccTree("BFCC" + juce::String(bfccIdx));
				addEventwiseStatistics(bfccTree, frame.bfccs[bfccIdx]);
				bfccsTree.addChild(bfccTree, bfccIdx, nullptr);
			}
			frameTree.addChild(bfccsTree, -1, nullptr);
			
			// Add single-value features
			ValueTree periodicityTree("Periodicity");
			addEventwiseStatistics(periodicityTree, frame.periodicity);
			frameTree.addChild(periodicityTree, -1, nullptr);
			
			ValueTree loudnessTree("Loudness");
			addEventwiseStatistics(loudnessTree, frame.loudness);
			frameTree.addChild(loudnessTree, -1, nullptr);
			
			ValueTree f0Tree("F0");
			addEventwiseStatistics(f0Tree, frame.f0);
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
		auto bfccsTree = frameTree.getChildWithName("BFCCs");
		if (bfccsTree.isValid())
		{
			frame.bfccs.reserve(bfccsTree.getNumChildren());
			
			for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
			{
				auto bfccTree = bfccsTree.getChild(bfccIdx);
				frame.bfccs.push_back(toEventwiseStatistics(bfccTree));
			}
		}
		
		// Extract single-value features
		auto periodicityTree = frameTree.getChildWithName("Periodicity");
		if (periodicityTree.isValid())
			frame.periodicity = toEventwiseStatistics(periodicityTree);
		
		auto loudnessTree = frameTree.getChildWithName("Loudness");
		if (loudnessTree.isValid())
			frame.loudness = toEventwiseStatistics(loudnessTree);
		
		auto f0Tree = frameTree.getChildWithName("F0");
		if (f0Tree.isValid())
			frame.f0 = toEventwiseStatistics(f0Tree);
		
		timbreSpace.push_back(std::move(frame));
	}
	
	return timbreSpace;
}

std::vector<float> valueTreeToNormalizedOnsets(juce::ValueTree const &vt)
{
	std::vector<float> normalizedOnsets;
	
	auto onsetArray = vt.getProperty("NormalizedOnsets");
	if (!onsetArray.isArray())
		return normalizedOnsets;
	
	auto* array = onsetArray.getArray();
	if (!array)
		return normalizedOnsets;
	
	normalizedOnsets.reserve(array->size());
	
	for (int i = 0; i < array->size(); ++i)
	{
		normalizedOnsets.push_back(static_cast<float>((*array)[i]));
	}
	
	return normalizedOnsets;
}

juce::var TimbreSpace::TreeManager::getOnsetsVar() const {
	return tree.getProperty("NormalizedOnsets");
}
juce::ValueTree TimbreSpace::TreeManager::getTimbralFramesTree() const {
	return tree.getChildWithName("TimbreMeasurements");
}
int TimbreSpace::TreeManager::getNumFrames() const {
	const auto& onsets = tree.getProperty("NormalizedOnsets");
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
	if (&treeWhichHasBeenChanged == &treeManager.tree){
		signalTimbreSpaceUpdated();
	}
}

void TimbreSpace::changeListenerCallback(juce::ChangeBroadcaster* source) {
	// could there be any reason to clear the tree? re-assigning it wouldn't need that, but what if the rest of this func fails? do we want a cleared tree at that point?
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
		if (!onsets.size()) {
			fmt::print("Onsets have 0 length");
			return;
		}
		setTimbreSpaceTree(timbreSpaceReprToVT(tspace, onsets, audioHash, _audioFileAbsPath));
		signalSaveAnalysisOption();
		signalTimbreSpaceUpdated();
	}
}
void TimbreSpace::fullSelfUpdate(bool verbose){
	if (verbose) {fmt::print("Extracting\n");}
	extract();
	if(verbose) {fmt::print("updating timbre points\n");}
	updateTimbreSpacePoints();
	if(verbose) {fmt::print("reshaping timbre space\n");}
	reshape(verbose);
	if(verbose) {fmt::print("computing delaunay triangulation\n");}
	_delaunator = std::make_unique<delaunator::Delaunator>(make2dCoordinates(timbres5D));
}
[[nodiscard]]
inline std::vector<analysis::Real>
extractFeatures(const juce::ValueTree& frameTree,
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
		int idx = static_cast<int>(f);
		Real value = 0.0f;
		
		if (0 <= idx && idx < NumBFCC) {
			// It's a BFCC - get from BFCCs array
			auto bfccsTree = frameTree.getChildWithName("BFCCs");
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
			
			auto scalarTree = frameTree.getChildWithName(childName);
			if (scalarTree.isValid()) {
				value = scalarTree.getProperty(statPropName, 0.0f);
			}
		}
		
		out.push_back(value);
	}
	
	return out;
}

void TimbreSpace::extract() {
	auto const &featuresToExtract = settings.dimensionWisefeatures;
	if (nvs::util::isEmpty(treeManager.tree)){
		std::cerr << "TimbreSpace::extract: timbre space empty, early exit\n";
		return;
	}
	eventwiseExtractedTimbrePoints.clear();
	eventwiseExtractedTimbrePoints.reserve(treeManager.getNumFrames());
	
	auto const &timbreTree = treeManager.getTimbralFramesTree();
	for (int i = 0; i < timbreTree.getNumChildren(); ++i) {
		ValueTree const &frame = timbreTree.getChild(i);
		std::vector<float> v = extractFeatures(frame, featuresToExtract, nvs::analysis::Statistic::Median);
		eventwiseExtractedTimbrePoints.push_back(v);
	}
}

std::vector<float> getHistoEqualizationVec(std::vector<float> const &points){
	std::vector<float> allX, vecOut;
	allX.reserve (points.size());
	vecOut.reserve (points.size());
	for (auto& p : points){
		allX.push_back (p);
	}
	std::sort (allX.begin(), allX.end());
	
	for (auto const& p : points)
	{
		// find first index ≥ p.x
		auto idx = (int)(std::lower_bound (allX.begin(),
										   allX.end(),
										   p)
						 - allX.begin());
		float quantile = (float) idx / (float)(allX.size() - 1);
		
		// optional: you can still gamma‑tweak the quantile
		float gamma    = 0.8f;             // <1 stretches mid‑values
		float t        = std::pow (quantile, gamma);
		vecOut.push_back(t);
	}
	return vecOut;
}

void TimbreSpace::updateTimbreSpacePoints()
{	/** to be called when the actual analyzed timbre space changes  */
	if (eventwiseExtractedTimbrePoints.size() == 0){
		std::cout << "updateAndDrawTimbreSpacePoints: timbreSpace empty, returning...\n";
		return;
	}
	{
		auto const n_dim = eventwiseExtractedTimbrePoints[0].size();
		_ranges.clear();
		_ranges.reserve(n_dim);
		
		for (size_t i = 0; i < n_dim; ++i){
			_ranges.push_back(nvs::analysis::calculateRangeOfDimension(eventwiseExtractedTimbrePoints, i));
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(eventwiseExtractedTimbrePoints.size());
		allDim1.reserve(eventwiseExtractedTimbrePoints.size());
		for (auto const& frame : eventwiseExtractedTimbrePoints){
			allDim0.push_back(frame[0]);	// e.g. bfcc1
			allDim1.push_back(frame[1]);	// e.g. bfcc2
		}
		histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
}
void TimbreSpace::reshape(bool verbose)
{   /** to be called when we only want to change the VIEW of the timbre points (which will also need to happen when the timbre space itself changes) */
    
    if (eventwiseExtractedTimbrePoints.size() == 0){     
        // writeToLog("drawTimbreSpacePoints: eventwiseExtractedTimbrePoints empty, returning...");
        return;
    }
    
    std::vector<std::vector<float>> const &timbreSpaceRepr = eventwiseExtractedTimbrePoints;
    if (!(timbreSpaceRepr[0].size() == _ranges.size())){
        // writeToLog("drawTimbreSpacePoints: point size mismatch, exiting early");
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
    
    auto squash = [](float xNorm) -> float { return std::asinh(10.0*xNorm) / (float)(M_PI); };
    auto foo = [&](float x, std::pair<float, float> range) -> float { return squash(normalizer(x, range)); };
    
    constexpr size_t nDim {5};
    
    // clear points of timbreSpaceHolds
    clear(); // clearing to make way for points we're about to be adding
    
    for (size_t i = 0; i < timbreSpaceRepr.size(); ++i) {
        std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
        
        jassert (timbreFrame.size() >= nDim);

        // ========================================2D========================================
        // squash normalized points within dimension range
        Timbre2DPoint pNL(foo(timbreFrame[0], _ranges[0]),
                          foo(timbreFrame[1], _ranges[1]));
        
        // histogram equalization
        float const &equalizedX = histoEqualizedD0[i];
        float const &equalizedY = histoEqualizedD1[i];
        
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
        // the Nth member of timbreSpaceComponent.timbres5D corresponds to
        // the Nth member of onsets.
        float const padding_scalar = 0.95f;
        add5DPoint(p * padding_scalar, color);
        
        if (verbose){
            fmt::print("adding the point {:.3f}, {:.3f}\n", p(0), p(1));
        }
    }
}

/**
 * Finds the K nearest points to `target` in `database`,
 * builds softmax-style weights over those K,
 * then either returns them all, or samples `numToPick` **without replacement**.
 *
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
    int                                numToPick,
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
          std::nth_element (
             distIdx.begin(),
             distIdx.begin() + K,
             distIdx.end(),
             [](auto& a, auto& b){ return a.distance < b.distance; });
          distIdx.resize (K);
          assert(distIdx.size() == (size_t)K);
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
       if (std::all_of(weightArr.begin(), weightArr.end(), [](double w) { return w == 0.0; })) {
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
		double uniformWeight = 1.0 / dv.size();
		weights.assign(dv.size(), uniformWeight);
	} else {
		// Apply softmax: exp(-sharpness * (distance/dmax))
		for (const auto& di : dv) {
			double normalizedDist = di.distance / dmax;
			weights.push_back(std::exp(-sharpness * normalizedDist));
		}
		
		// Normalize to sum = 1
		double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
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
		double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		if (sum > 0.0) {
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

// New triangulation-based point selection function
std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target, const juce::Array<Timbre5DPoint>& database, const delaunator::Delaunator &d)
{
	if (database.isEmpty()) { return {}; }
	
	// Need at least 3 points for triangulation
	if (database.size() < 3) {
		// Fall back to distance-based method or return all points with equal weights
		std::vector<util::WeightedIdx> result;
		double weight = 1.0 / database.size();
		for (int i = 0; i < database.size(); ++i) {
			result.emplace_back(i, weight);
		}
		return result;
	}
	
	// Find triangle containing the target point
	const Timbre2DPoint targetPoint = get2D(target);
	const auto triangleOpt = findContainingTriangle(d, targetPoint);
	
	if (!triangleOpt.has_value()) {
		// Target point is outside the convex hull
		// Fall back to distance-based method or handle as edge case
		return findNearestTrianglePoints(target, database, d);
	}
	
	// Get the triangle vertices
	const auto triangle = triangleOpt.value();
	const size_t idx0 = triangle[0];
	const size_t idx1 = triangle[1];
	const size_t idx2 = triangle[2];

    jassert (idx0 < database.size());
    jassert (idx1 < database.size());
    jassert (idx2 < database.size());

	// Get the 2D points for barycentric calculation
	Timbre2DPoint p0 = get2D(database.getReference(idx0));
	Timbre2DPoint p1 = get2D(database.getReference(idx1));
	Timbre2DPoint p2 = get2D(database.getReference(idx2));
	
	// Compute barycentric weights
	auto weights = computeBarycentricWeights(targetPoint, p0, p1, p2);
	
	// Create result vector
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(idx0, weights[0]);
	result.emplace_back(idx1, weights[1]);
	result.emplace_back(idx2, weights[2]);

	return result;
}

// Helper function for when target is outside convex hull
std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const juce::Array<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d)
{
	// Find the nearest edge or vertex of the convex hull
	// This is a simplified approach - you might want to be more sophisticated
	
	Timbre2DPoint targetPoint = get2D(target);
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle;
	
	// Check all triangles and find the one with minimum distance to target
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		size_t idx0 = d.triangles[i];
		size_t idx1 = d.triangles[i + 1];
		size_t idx2 = d.triangles[i + 2];
		
		Timbre2DPoint p0 = get2D(database.getReference(idx0));
		Timbre2DPoint p1 = get2D(database.getReference(idx1));
		Timbre2DPoint p2 = get2D(database.getReference(idx2));
		
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
