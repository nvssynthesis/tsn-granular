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

vecVecReal getBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory, analysisSettings const anSettings, bfccSettings const bfSettings)
{
	size_t const waveSize = waveSpan.size();
	vecReal wave(waveSize);
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	bfccSettings::spectrumType_e specType = bfSettings.specType;
	std::string const specAlgoStr  = (specType == bfccSettings::spectrumType_e::power) ? "PowerSpectrum" : "Spectrum";
	std::string const specInputStr = (specType == bfccSettings::spectrumType_e::power) ? "signal" : "frame";
	std::string const specOutputStr = (specType == bfccSettings::spectrumType_e::power) ? "powerSpectrum" : "spectrum";

	
	vectorInput *inVec = new vectorInput(&wave);
	
	Algorithm* frameCutter	=	factory.create("FrameCutter",
											   "frameSize", anSettings.frameSize,
											   "hopSize", anSettings.hopSize,
											   "lastFrameToEndOfFile", true,
											   "silentFrames", "keep",
											   "startFromZero", true,
											   "validFrameThresholdRatio", 0.f);
	
	Algorithm* windowing	=	factory.create("Windowing",
											   "normalized", false,
											   "size", anSettings.frameSize,
											   "zeroPadding", anSettings.frameSize,
											   "type", "hann",
											   "zeroPhase", false);
	
	Algorithm* spectrum 	= 	factory.create(specAlgoStr,
											   "size", anSettings.frameSize * 2);
	
	Algorithm*  bfcc 		= 	factory.create("BFCC",
												"dctType", bfSettings.dctType,
												"highFrequencyBound", bfSettings.highFrequencyBound,
												"inputSize", anSettings.frameSize + 1,
												"liftering", bfSettings.liftering,
												"logType", "dbamp",	// logarithmic compression type. Use 'dbpow' if working with power and 'dbamp' if working with magnitudes
												"lowFrequencyBound", bfSettings.lowFrequencyBound,
												"normalize", bfSettings.normalizeTypeAsString,
												"numberBands", bfSettings.numBands,
												"numberCoefficients", bfSettings.numCoefficients,
												"sampleRate", anSettings.sampleRate,
												"type", bfSettings.spectrumTypeAsString,
												"weighting", bfSettings.weightingType
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
	return BFCCs[0];	// the only dimension that was used
}

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory){
	namespace ess_std = essentia::standard;
	
	constexpr int nDim = 6;
	const std::string namespaceIn {"BFCC"};
	const std::string namespaceOut {"BFCC pca"};


	ess_std::Algorithm* PCA = factory.create("PCA",
											 "dimensions", nDim,
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

std::pair<Real, Real> getRangeOfDimension(vecVecReal const &V, size_t dim){
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

Real getNormalizationMultiplier(std::pair<Real, Real> range){
	return 1.f / std::max(std::abs(range.first), std::abs(range.second));
}

}	// namespace analysis
}	// namespace nvs
