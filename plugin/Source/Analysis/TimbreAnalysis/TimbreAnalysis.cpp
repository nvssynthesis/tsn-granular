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
std::vector<float> calculatePitchesAubioYinFast(std::span<Real> waveSpan, AnalyzerSettings const& settings){
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

PitchesAndConfidences calculatePitchesEssentiaYin(std::span<Real> waveSpan, streamingFactory const &factory, AnalyzerSettings const& settings){
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	vectorInput *inVec = new vectorInput(&wave);
	
	int const frameSize = settings.analysis.frameSize;
	int const hopSize = settings.analysis.hopSize;
	
	int const zeroPadding = frameSize;

	constexpr double validFrameThresholdRatio = 0.f;
	
	Algorithm* frameCutter = nvs::analysis::streamingFactory::create ("FrameCutter",
		"frameSize",            frameSize,
		"hopSize",              hopSize,
		"lastFrameToEndOfFile", true,
		"silentFrames",         "keep",
		"startFromZero",        true,
		"validFrameThresholdRatio", validFrameThresholdRatio
	);
	

	Algorithm* windowing = nvs::analysis::streamingFactory::create ("Windowing",
		"normalized", false,
		"size",        frameSize,
		"zeroPadding", zeroPadding,
		"type",        settings.analysis.windowingType.toStdString(),
		"zeroPhase",   false
	);
	
	float sr = settings.analysis.sampleRate;
	jassert (sr > 20000.f);
	auto maxFrequency = settings.pitch.maxFrequency;
	auto minFrequency = settings.pitch.minFrequency;
	auto tolerance = settings.pitch.tolerance;
	
	std::string pitchAlgoStr = settings.pitch.pitchDetectionAlgorithm.toStdString();
	
	std::map<std::string, std::string> pitchAlgoNicknameMap {
		{"yin", "PitchYin"}
	};	// for now we only handle this
	
	Algorithm* pitchDet = nvs::analysis::streamingFactory::create (pitchAlgoNicknameMap[pitchAlgoStr],
		"frameSize",   frameSize,
		"interpolate",  settings.pitch.interpolate,
		"maxFrequency", maxFrequency,
		"minFrequency", minFrequency,
		"sampleRate",   sr,
		"tolerance",    tolerance
	);
	
	Algorithm* pitchFrameAccumulator = nvs::analysis::streamingFactory::create("RealAccumulator");
	Algorithm* pitchConfidenceFrameAccumulator = nvs::analysis::streamingFactory::create("RealAccumulator");
	
	std::vector<vecReal> pitches, pitchConfidences;
	auto *pitchAccumOutput = new VectorOutput<vecReal>(&pitches);
	auto *pitchConfidenceAccumOutput = new VectorOutput<vecReal>(&pitchConfidences);
	
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
													 AnalyzerSettings const& settings)
{
	auto const algo      = settings.pitch.pitchDetectionAlgorithm.toStdString();

	if (algo == "yin") {
		return calculatePitchesEssentiaYin (waveEvent, factory, settings);
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

vecReal calculateLoudnesses(std::span<Real const> waveSpan, streamingFactory const &factory, AnalyzerSettings const& settings)
{
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	float sampleRate  = settings.analysis.sampleRate;

	int    frameSize  = settings.analysis.frameSize;
	int    hopSize    = settings.analysis.hopSize;

	vectorInput *inVec = new vectorInput(&wave);
//	Algorithm* equalLoudnessFilter = factory.create(
//		"EqualLoudness",
//		"sampleRate", sampleRate
//	);	// not using yet 
	Algorithm* frameCutter = nvs::analysis::streamingFactory::create (
		"FrameCutter",
		"frameSize",               frameSize,
		"hopSize",                 hopSize,
		"lastFrameToEndOfFile",    true,
		"silentFrames",            "keep",
		"startFromZero",           true,
		"validFrameThresholdRatio", 0.0
	);
	
	Algorithm* windowing = nvs::analysis::streamingFactory::create (
		"Windowing",
		  "normalized",  false,
		  "size",        frameSize,
		  "zeroPadding", frameSize,
		  "type",        settings.analysis.windowingType.toStdString(),
		  "zeroPhase",   false
	);
	Algorithm* loudness = nvs::analysis::streamingFactory::create("Loudness");
	
	Algorithm* loudnessFrameAccumulator = nvs::analysis::streamingFactory::create("RealAccumulator");
	
	std::vector<vecReal> loudnesses;
	auto *pitchAccumOutput = new VectorOutput<vecReal>(&loudnesses);
	
	*inVec												>>		frameCutter->input("signal");
	frameCutter->output("frame")						>>		windowing->input("frame");
	windowing->output("frame")							>>		loudness->input("signal");
	loudness->output("loudness")						>>		loudnessFrameAccumulator->input("data");
	loudnessFrameAccumulator->output("array")			>>		pitchAccumOutput->input("data");
	
	Network n(inVec);
	n.run();
	n.clear();
	
	assert(loudnesses.size() == 1);
	
	return loudnesses[0];
}

vecVecReal calculateBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory, AnalyzerSettings const& settings)
{
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	float const sampleRate  = settings.analysis.sampleRate;
	
	int const  frameSize  = settings.analysis.frameSize;
	int const  hopSize    = settings.analysis.hopSize;

	// Compute the spectrum→algorithm mapping from the BFCC tree:
	auto const spectrumTypeStr = settings.bfcc.spectrumType.toStdString();
	// If your stored property is e.g. "power" or "magnitude", branch on that:
	bool const isPower = (spectrumTypeStr == "power");

	std::string const specAlgoStr   = isPower ? "PowerSpectrum" : "Spectrum";
	std::string const specInputStr  = isPower ? "signal"        : "frame";
	std::string const specOutputStr = isPower ? "powerSpectrum" : "spectrum";
	
	
	vectorInput *inVec = new vectorInput(&wave);
	
	Algorithm* frameCutter = nvs::analysis::streamingFactory::create (
		"FrameCutter",
		  "frameSize",               frameSize,
		  "hopSize",                 hopSize,
		  "lastFrameToEndOfFile",    true,
		  "silentFrames",            "keep",
		  "startFromZero",           true,
		  "validFrameThresholdRatio", 0.0
	);
	
	Algorithm* windowing = nvs::analysis::streamingFactory::create (
		"Windowing",
		  "normalized",  false,
		  "size",        frameSize,
		  "zeroPadding", frameSize,
		  "type",        settings.analysis.windowingType.toStdString(),
		  "zeroPhase",   false
	);
	
	Algorithm* spectrum = nvs::analysis::streamingFactory::create (
		specAlgoStr,
		  "size", frameSize * 2
	);
	
	
	std::map<juce::String, int> dctTypeStringToInt {
		{ "typeII",  2 },
		{ "typeIII", 3 }
	};
	std::map<std::string, std::string> logTypeMap {
		{"PowerSpectrum", "dbpow"},
		{"Spectrum", "dbamp"}
	};
	
	Algorithm* bfcc = nvs::analysis::streamingFactory::create (
		"BFCC",
		  "dctType",             dctTypeStringToInt.at(settings.bfcc.dctType.toStdString()),
		  "highFrequencyBound",  settings.bfcc.highFrequencyBound,
		  "inputSize",           frameSize + 1,
		  "liftering",           settings.bfcc.liftering,
		  "logType",             logTypeMap.at(specAlgoStr),
									  
		  "lowFrequencyBound",   settings.bfcc.lowFrequencyBound,
		  "normalize",           settings.bfcc.normalize.toStdString(),
		  "numberBands",         settings.bfcc.numBands,
		  "numberCoefficients",  settings.bfcc.numCoefficients,
		  "sampleRate",          sampleRate,
		  "type",                spectrumTypeStr,
		  "weighting",           settings.bfcc.weightingType.toStdString()
	);
	
	Algorithm* barkFrameAccumulator = nvs::analysis::streamingFactory::create("VectorRealAccumulator");
	Algorithm* bfccFrameAccumulator = nvs::analysis::streamingFactory::create("VectorRealAccumulator");
	
	// for some reason, to get this working in as a connection to FrameAccumulator,
	// these must be vector<vector<vector<Real>>>, and the 1st dimension only has size of 1...
	std::vector<std::vector<vecReal>> barkBands, BFCCs;
	auto *barkAccumOutput = new VectorOutput<std::vector<vecReal>>(&barkBands);
	auto *bfccAccumOutput = new VectorOutput<std::vector<vecReal>>(&BFCCs);
	
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
	assert(!BFCCs[0].empty());
	assert(!BFCCs[0][0].empty());

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

	ess_std::Algorithm* PCA = nvs::analysis::standardFactory::create("PCA",
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
