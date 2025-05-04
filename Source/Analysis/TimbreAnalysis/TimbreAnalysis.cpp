/*
  ==============================================================================

    TimbreAnalysis.cpp
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreAnalysis.h"

namespace nvs {
namespace analysis {

namespace {
std::vector<float> calculatePitchesAubioYinFast(std::span<Real> waveSpan, juce::ValueTree settingsTree){
#ifdef INCLUDE_AUBIO
	size_t const N = waveSpan.size();
	
	int const num_wins = (N <= win_sz) ? 1 : ceil(N - win_sz) / hop_sz + 1;
	
	auto presetInfoTree = settingsTree.getParent().getChildWithName("PresetInfo");
	auto analysisTree = settingsTree.getChildWithName("Analysis");
	
	aubio_pitch_t *pDetector = new_aubio_pitch("yinfast",
		/*buf_size*/ analysisTree.getProperty("frameSize").toInt(),
		/*hop_size*/ analysisTree.getProperty("hopSize").toInt(),
		/*sample_rate*/ presetInfoTree.getProperty("sampleRate").toFloat());

	if (!pDetector){
		return {};
	}
	
	if (aubio_pitch_set_silence(pDetector, -50.f)) { // returns 0 on success
		del_aubio_pitch(pDetector);
		return {};
	}
	
	if (aubio_pitch_set_unit(pDetector, "cent")) {
		del_aubio_pitch(pDetector);
		return {};
	}
	
	std::vector<float> pitches(num_wins, 0.0f);
	
	fvec_t* fv_in = new_fvec(win_sz);
	fvec_t* fv_out = new_fvec(1);
	
	if (!fv_in || !fv_out) {
		del_fvec(fv_in);
		del_fvec(fv_out);
		del_aubio_pitch(pDetector);
		return {};
	}
	
	for (int i = 0; i < num_wins; ++i) {
		std::memcpy(fv_in->data,
					waveSpan.data() + i * hop_sz,
					win_sz * sizeof(Real));
		
		aubio_pitch_do(pDetector, fv_in, fv_out);
		
		pitches[i] = fvec_get_sample(fv_out, 0);
	}
	
	del_fvec(fv_in);
	del_fvec(fv_out);
	del_aubio_pitch(pDetector);
	
	return pitches;
#else
	assert(false);
	return {};	// of course it won't return anyway.
#endif
}

PitchesAndConfidences calculatePitchesEssentiaYin(std::span<Real> waveSpan, streamingFactory const &factory, juce::ValueTree settingsTree){
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	vectorInput *inVec = new vectorInput(&wave);
	
	auto presetInfoTree = settingsTree.getParent().getChildWithName("PresetInfo");
	auto analysisTree = settingsTree.getChildWithName ("Analysis");
	auto pitchTree    = settingsTree.getChildWithName ("Pitch");
	auto bfccTree = settingsTree.getChildWithName ("BFCC");

	
	Algorithm* frameCutter = factory.create ("FrameCutter",
		"frameSize",            (int)  analysisTree.getProperty("frameSize"),
		"hopSize",              (int)  analysisTree.getProperty("hopSize"),
		"lastFrameToEndOfFile", true,
		"silentFrames",         "keep",
		"startFromZero",        true,
		"validFrameThresholdRatio", (double) analysisTree.getProperty("validFrameThresholdRatio")
	);
	

	Algorithm* windowing = factory.create ("Windowing",
		"normalized", false,
		"size",        (int)    analysisTree.getProperty("frameSize"),
		"zeroPadding", (int)    analysisTree.getProperty("frameSize"),
		"type",        analysisTree.getProperty("windowingType").toString().toStdString(),
		"zeroPhase",   false
	);
	
	float sr = (float) presetInfoTree.getProperty("sampleRate");
	jassert (sr > 20000.f);
	auto maxFrequency = (float) pitchTree.getProperty ("maxFrequency");
	auto minFrequency = (float) pitchTree.getProperty ("minFrequency");
	auto tolerance = (float) pitchTree.getProperty ("tolerance");
	
	std::string pitchAlgoStr = pitchTree.getProperty ("pitchDetectionAlgorithm").toString().toStdString();
	
	std::map<std::string, std::string> pitchAlgoNicknameMap {
		{"yin", "PitchYin"}
	};	// for now we only handle this
	
	Algorithm* pitchDet = factory.create (pitchAlgoNicknameMap[pitchAlgoStr],
		"frameSize",    (int)    analysisTree.getProperty ("frameSize"),
		"interpolate",  (bool)   pitchTree.getProperty ("interpolate"),
		"maxFrequency", maxFrequency,
		"minFrequency", minFrequency,
		"sampleRate",   sr,
		"tolerance",    tolerance
	);
	
	Algorithm* pitchFrameAccumulator = factory.create("RealAccumulator");
	Algorithm* pitchConfidenceFrameAccumulator = factory.create("RealAccumulator");
	
	std::vector<vecReal> pitches, pitchConfidences;
	VectorOutput<vecReal> *pitchAccumOutput = new VectorOutput<vecReal>(&pitches);
	VectorOutput<vecReal> *pitchConfidenceAccumOutput = new VectorOutput<vecReal>(&pitchConfidences);
	
	*inVec												>>		frameCutter->input("signal");
	frameCutter->output("frame")						>>		windowing->input("frame");
	windowing->output("frame")							>>		pitchDet->input("signal");
	pitchDet->output("pitch")							>>		pitchFrameAccumulator->input("data");
	pitchDet->output("pitchConfidence")					>>		pitchConfidenceFrameAccumulator->input("data");
	pitchFrameAccumulator->output("array")				>>		pitchAccumOutput->input("data");
	pitchConfidenceFrameAccumulator->output("array")	>>		pitchConfidenceAccumOutput->input("data");
	
	Network n(inVec);
	n.run();
	n.clear();
	
	assert(pitches.size() == pitchConfidences.size());
	assert(pitches.size() == 1);
	assert(pitches[0].size() == pitchConfidences[0].size());
	
	return PitchesAndConfidences { pitches[0], pitchConfidences[0] };
}
}	// anonymous namespace

PitchesAndConfidences calculatePitchesAndConfidences (vecReal waveEvent,
													 streamingFactory const& factory,
													 juce::ValueTree settingsTree)
{
	auto pitchTree = settingsTree.getChildWithName ("Pitch");
	auto algo      = pitchTree.getProperty ("pitchDetectionAlgorithm").toString();

	if (algo == "yin") {
		return calculatePitchesEssentiaYin (waveEvent, factory, settingsTree);
	}
	else if (algo == "pYin") {
		jassertfalse;  // not implemented
		return {};
//		return calculatePitchesEssentiaPYin (waveEvent, factory, settingsTree);
	}
	else if (algo == "chroma") {
		jassertfalse;  // not implemented
		return {};
//		return calculatePitchesEssentiaChroma (waveEvent, factory, settingsTree);
	}
	else {
		jassertfalse;  // unknown algorithm
		return {};
	}
}

vecVecReal calculateBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory, juce::ValueTree settingsTree)
{
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	float sampleRate  = (float) settingsTree.getParent().getChildWithName("PresetInfo").getProperty ("sampleRate");

	auto analysisTree = settingsTree.getChildWithName ("Analysis");
	auto bfccTree     = settingsTree.getChildWithName ("BFCC");
	
	 int    frameSize  = (int)    analysisTree.getProperty ("frameSize");
	 int    hopSize    = (int)    analysisTree.getProperty ("hopSize");

	// Compute the spectrum→algorithm mapping from the BFCC tree:
	auto spectrumTypeStr = ((juce::String) bfccTree.getProperty ("spectrumType")).toStdString();
	// If your stored property is e.g. "power" or "magnitude", branch on that:
	bool isPower = (spectrumTypeStr == "power");

	std::string specAlgoStr   = isPower ? "PowerSpectrum" : "Spectrum";
	std::string specInputStr  = isPower ? "signal"        : "frame";
	std::string specOutputStr = isPower ? "powerSpectrum" : "spectrum";
	
	
	vectorInput *inVec = new vectorInput(&wave);
	
	Algorithm* frameCutter = factory.create (
		"FrameCutter",
		  "frameSize",               frameSize,
		  "hopSize",                 hopSize,
		  "lastFrameToEndOfFile",    true,
		  "silentFrames",            "keep",
		  "startFromZero",           true,
		  "validFrameThresholdRatio", 0.0
	);
	
	jassert (analysisTree.hasProperty("windowingType"));
	std::string windowType = analysisTree.getProperty("windowingType").toString().toStdString();
	Algorithm* windowing = factory.create (
		"Windowing",
		  "normalized",  false,
		  "size",        frameSize,
		  "zeroPadding", frameSize,
		  "type",        windowType,
		  "zeroPhase",   false
	);
	
	Algorithm* spectrum = factory.create (
		specAlgoStr,
		  "size", frameSize * 2
	);
	
	
	std::map<juce::String, int> dctTypeStringToInt {
		{ "typeII",  2 },
		{ "typeIII", 3 }
	};
	
	Algorithm* bfcc = factory.create (
		"BFCC",
		  "dctType",             dctTypeStringToInt[bfccTree.getProperty ("dctType").toString()],
		  "highFrequencyBound",  (double) bfccTree.getProperty ("highFrequencyBound"),
		  "inputSize",           frameSize + 1,
		  "liftering",           (int)    bfccTree.getProperty ("liftering"),
		  "logType",             std::string ("dbamp"),
		  "lowFrequencyBound",   (double) bfccTree.getProperty ("lowFrequencyBound"),
		  "normalize",           ((juce::String) bfccTree.getProperty ("normalize")).toStdString(),
		  "numberBands",         (int)    bfccTree.getProperty ("numBands"),
		  "numberCoefficients",  (int)    bfccTree.getProperty ("numCoefficients"),
		  "sampleRate",          sampleRate,
		  "type",                ((juce::String) bfccTree.getProperty ("spectrumType")).toStdString(),
		  "weighting",           ((juce::String) bfccTree.getProperty ("weightingType")).toStdString()
	);
	
	Algorithm* barkFrameAccumulator = factory.create("VectorRealAccumulator");
	Algorithm* bfccFrameAccumulator = factory.create("VectorRealAccumulator");
	
	// for some reason, to get this working in as a connection to FrameAccumulator,
	// these must be vector<vector<vector<Real>>>, and the 1st dimension only has size of 1...
	std::vector<std::vector<vecReal>> barkBands, BFCCs;
	VectorOutput<std::vector<vecReal>> *barkAccumOutput = new VectorOutput<std::vector<vecReal>>(&barkBands);
	VectorOutput<std::vector<vecReal>> *bfccAccumOutput = new VectorOutput<std::vector<vecReal>>(&BFCCs);
	
	*inVec									>>		frameCutter->input("signal");
	frameCutter->output("frame")			>>		windowing->input("frame");
	windowing->output("frame")				>>		spectrum->input(specInputStr);
	spectrum->output(specOutputStr)			>>		bfcc->input("spectrum");
	bfcc->output("bands")					>>		barkFrameAccumulator->input("data");
	bfcc->output("bfcc")					>>		bfccFrameAccumulator->input("data");
	barkFrameAccumulator->output("array")	>>		*barkAccumOutput;
	bfccFrameAccumulator->output("array")	>>		*bfccAccumOutput;
	
	Network n(inVec);
	
	n.run();
	n.clear();
	
	assert(BFCCs.size() == 1);
	assert(BFCCs[0].size());
	assert(BFCCs[0][0].size());

	// truncate by removing 1st BFCC (which just encodes overall energy)
#if 0
	for (auto &frame : BFCCs){
		size_t const preSz = frame.size();
		frame.erase(frame.begin());
		assert(frame.size() < preSz);
	}
#endif
	// normalize by 0th BFCC
#if 0
	for (std::vector<float> &frame : BFCCs[0]){
		for (int i=1; i < frame.size(); ++i){
			frame[i] /= frame[0];
		}
	}
#endif
	return BFCCs[0];	// the only dimension that was used
}

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory, int num_features_out){
	namespace ess_std = essentia::standard;
	
	const std::string namespaceIn {"data"};
	const std::string namespaceOut {"pca"};

	ess_std::Algorithm* PCA = factory.create("PCA",
											 "dimensions", num_features_out,
											 "namespaceIn", namespaceIn,
											 "namespaceOut", namespaceOut);
	
	essentia::Pool inPool, outPool;
	for (auto v : V){
		inPool.add(namespaceIn, v);
	}
	
	PCA->input("poolIn").set(inPool);
	PCA->output("poolOut").set(outPool);
	PCA->compute();
	
	auto const &vecRealPool = outPool.getVectorRealPool();
	vecVecReal PCAmat = vecRealPool.at(namespaceOut);
	
	return PCAmat;
}

std::pair<Real, Real> calculateRangeOfDimension(vecVecReal const &V, size_t dim){
	Real min {std::numeric_limits<Real>::max()};
	Real max {std::numeric_limits<Real>::lowest()};
	
	for (auto const &v : V){
		auto const val = v[dim];
		if (val < min){
			min = val;
		}
		if (val > max){
			max = val;
		}
	}
	
	return std::make_pair(min, max);
}

Real calculateNormalizationMultiplier(std::pair<Real, Real> range){
	return 1.f / std::max(std::abs(range.first), std::abs(range.second));
}

}	// namespace analysis
}	// namespace nvs
