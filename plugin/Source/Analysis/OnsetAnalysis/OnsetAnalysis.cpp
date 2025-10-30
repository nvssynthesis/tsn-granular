/*
  ==============================================================================

    OnsetAnalysis.cpp
    Created: 14 Jun 2023 12:16:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "OnsetAnalysis.h"

/** TODO:
 consolidate onsetsInSeconds with onsetAnalysis.
 The trouble is that there was a problem figuring out how to use essentia's streaming factory (instead of standard) with the Onsets algorithm. So that function has to reference a different factory.
 */

namespace nvs::analysis {

vecReal makeSweptSine(Real const low, Real const high, size_t const len, Real const sampleRate){
	std::vector<Real> freqSweep(len, 0.f);
	for (size_t i = 0; i < len; ++i){
		const Real x = (high - low) * (static_cast<Real>(i) / static_cast<Real>(len)) + low;
		freqSweep[i] = std::sin(2.f * 3.14159265f * (x / sampleRate));
	}
	
	return freqSweep;
}
static vecReal getWeights(const AnalyzerSettings &settings) {
    return {
        static_cast<float>(settings.onset.weight_hfc),
        static_cast<float>(settings.onset.weight_complex),
        static_cast<float>(settings.onset.weight_complexPhase),
        static_cast<float>(settings.onset.weight_flux),
        static_cast<float>(settings.onset.weight_melFlux),
        static_cast<float>(settings.onset.weight_rms)
    };
}
array2dReal calculateOnsetsMatrix(std::vector<Real> const &waveform,
						  streamingFactory const &factory,
						  AnalyzerSettings const &settings,
						  RunLoopStatus& rls,
						  const ShouldExitFn &shouldExit)
{
	const auto input_sr     = settings.analysis.sampleRate;
	assert(0.0 < input_sr);
    constexpr auto internal_sr  = 44100.0f;
	const auto frameSize    = std::min(std::max(512, settings.analysis.frameSize), 2048);
    constexpr auto frameSize1024= 1024; // dedicated for melflux
	constexpr auto hopSize      = 512;

	const bool needSeparateMelFluxChain = (frameSize != frameSize1024);


	auto *inVec = new vectorInput(&waveform);

	Algorithm* resampler	= factory.create("Resample",
											 "inputSampleRate", input_sr,
											 "outputSampleRate", internal_sr,
											 "quality",	2);	/* quality: SRC_SINC_FASTEST
															 from enum {
																	   SRC_SINC_BEST_QUALITY       = 0,
																	   SRC_SINC_MEDIUM_QUALITY     = 1,
																	   SRC_SINC_FASTEST            = 2,
																	   SRC_ZERO_ORDER_HOLD         = 3,
																	   SRC_LINEAR                  = 4
																   } ;
															 */

	// ============ Main processing chain (variable frameSize) ============
	Algorithm* frameCutter  = factory.create("FrameCutter",
											 "frameSize", frameSize,
											 "hopSize", hopSize,
											 "startFromZero", false,
											 "lastFrameToEndOfFile", true,
											 "validFrameThresholdRatio", 0.0f);

	Algorithm* windowingToFFT = factory.create("Windowing",
											 "normalized", true,
											 "size", frameSize,
											 "zeroPhase", false,
											 "zeroPadding", frameSize,
											 "type", "hamming");

	Algorithm* FFT			= factory.create("FFT",
											 "size", frameSize);

	Algorithm* carToPol		= factory.create("CartesianToPolar");

	// ============ Separate processing chain for MelFlux (only if frameSize != 1024) ============
	Algorithm* frameCutter1024 = nullptr;
	Algorithm* windowingToFFT1024 = nullptr;
	Algorithm* FFT1024 = nullptr;
	Algorithm* carToPol1024 = nullptr;

	if (needSeparateMelFluxChain) {
		frameCutter1024  = factory.create("FrameCutter",
												 "frameSize", frameSize1024,
												 "hopSize", hopSize,
												 "startFromZero", false,
												 "lastFrameToEndOfFile", true,
												 "validFrameThresholdRatio", 0.0f);

		windowingToFFT1024 = factory.create("Windowing",
												 "normalized", true,
												 "size", frameSize1024,
												 "zeroPhase", false,
												 "zeroPadding", frameSize1024,
												 "type", "hamming");

		FFT1024		= factory.create("FFT",
												 "size", frameSize1024);

		carToPol1024	= factory.create("CartesianToPolar");
	}

    // ============ Connect beginning of chain (independent of which onset detectors are used) ============
    *inVec >> resampler->input("signal");
    resampler->output("signal") >> frameCutter->input("signal");
    frameCutter->output("frame") >> windowingToFFT->input("frame");
    windowingToFFT->output("frame") >> FFT->input("frame");
    FFT->output("fft") >> carToPol->input("complex");


	// ============ Connect individual onset detection algorithms ============
    vecReal onsetDetVecHFC, onsetDetVecComplex, onsetDetVecComplexPhase, onsetDetVecFlux, onsetDetVecMelFlux, onsetDetVecRms;
    std::vector<std::reference_wrapper<vecReal>> detectionRefs {
        onsetDetVecHFC,
        onsetDetVecComplex,
        onsetDetVecComplexPhase,
        onsetDetVecFlux,
        onsetDetVecMelFlux,
        onsetDetVecRms
    };

    const auto weights = getWeights(settings);

    if (const auto weightSum = std::accumulate(weights.begin(), weights.end(), 0.f);
        weightSum == 0.f)
    {
        jassertfalse;
        // handle case where user asked for cumulative weight of 0
    }

    if (0.f < settings.onset.weight_hfc) {
        Algorithm* onsetDetectionHfc = factory.create("OnsetDetection",
                                                "method", "hfc",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionHfc->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionHfc->input("phase");
        auto *onsetDetsHFC = new vectorOutput(&onsetDetVecHFC);
        onsetDetectionHfc->output("onsetDetection") >> *onsetDetsHFC;
    }
    if (0.f < settings.onset.weight_complex) {
        Algorithm* onsetDetectionComplex = factory.create("OnsetDetection",
                                                "method", "complex",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionComplex->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionComplex->input("phase");
        auto *onsetDetsComplex = new vectorOutput(&onsetDetVecComplex);
        onsetDetectionComplex->output("onsetDetection") >> *onsetDetsComplex;
    }
    if (0.f < settings.onset.weight_complexPhase) {
        Algorithm* onsetDetectionComplexPhase = factory.create("OnsetDetection",
                                                "method", "complex_phase",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionComplexPhase->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionComplexPhase->input("phase");
        auto *onsetDetsComplexPhase = new vectorOutput(&onsetDetVecComplexPhase);
        onsetDetectionComplexPhase->output("onsetDetection") >> *onsetDetsComplexPhase;
    }
    if (0.f < settings.onset.weight_flux) {
        Algorithm* onsetDetectionFlux = factory.create("OnsetDetection",
                                                "method", "flux",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionFlux->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionFlux->input("phase");
        auto *onsetDetsFlux = new vectorOutput(&onsetDetVecFlux);
        onsetDetectionFlux->output("onsetDetection") >> *onsetDetsFlux;
    }
    if (0.f < settings.onset.weight_melFlux) {
        Algorithm* onsetDetectionMelFlux = factory.create("OnsetDetection",
                                                "method", "melflux",
                                                   "sampleRate", internal_sr);
        // ============ Connect MelFlux to appropriate chain ============
        if (needSeparateMelFluxChain) {
            // Use separate 1024 chain for MelFlux
            resampler->output("signal") >> frameCutter1024->input("signal");
            frameCutter1024->output("frame") >> windowingToFFT1024->input("frame");
            windowingToFFT1024->output("frame") >> FFT1024->input("frame");
            FFT1024->output("fft") >> carToPol1024->input("complex");

            carToPol1024->output("magnitude") >> onsetDetectionMelFlux->input("spectrum");
            carToPol1024->output("phase")	>> onsetDetectionMelFlux->input("phase");
        } else {
            // Use main chain for MelFlux (frameSize is already 1024)
            carToPol->output("magnitude") >> onsetDetectionMelFlux->input("spectrum");
            carToPol->output("phase")	>> onsetDetectionMelFlux->input("phase");
        }
        auto *onsetDetsMelFlux = new vectorOutput(&onsetDetVecMelFlux);
        onsetDetectionMelFlux->output("onsetDetection") >> *onsetDetsMelFlux;
    }
    if (0.f < settings.onset.weight_rms) {
        Algorithm* onsetDetectionRms = factory.create("OnsetDetection",
                                                "method", "rms",
                                                   "sampleRate", internal_sr);
        auto *onsetDetsRms = new vectorOutput(&onsetDetVecRms);
        carToPol->output("magnitude") >> onsetDetectionRms->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionRms->input("phase");
        onsetDetectionRms->output("onsetDetection") >> *onsetDetsRms;
    }
	
	Network n(inVec);
	n.runPrepare();
	rls.set(0.0);
	rls.set("Computing onset matrix...");
	while (n.runStep()){
		if (shouldExit()) {
			break;
		}
	}
	rls.set(1.0);
	n.clear();

    // this bit just takes all the detection outputs and makes them constant-size (which should only not happen if some of them had a weight of 0)
    const auto correctSizedVec = std::ranges::max_element(detectionRefs,
                                                          [](const vecReal &v0, const vecReal &v1)
                                                          {
                                                              return v0.size() < v1.size();
                                                          });
    const size_t correctSize = correctSizedVec->get().size();

    jassert (0 < correctSize);
    for (auto d : detectionRefs) {
        if (d.get().empty()) {
            d.get() = vecReal(correctSize, 0.f);
        }
        jassert (d.get().size() == correctSize);
    }

	TNT::Array2D<essentia::Real> onsetsMatrix(6, static_cast<int>(correctSize));
	for (size_t j = 0; j < correctSize; ++j){
		onsetsMatrix[0][j] = onsetDetVecHFC[j];
		onsetsMatrix[1][j] = onsetDetVecComplex[j];
		onsetsMatrix[2][j] = onsetDetVecComplexPhase[j];
		onsetsMatrix[3][j] = onsetDetVecFlux[j];
		onsetsMatrix[4][j] = onsetDetVecMelFlux[j];
		onsetsMatrix[5][j] = onsetDetVecRms[j];
	}
	return onsetsMatrix;
}

#pragma message("make this work with StreamingFactory")
vecReal calculateOnsetsInSeconds(const array2dReal &onsetAnalysisMatrix,
								 const standardFactory &factory,
								 const AnalyzerSettings &settings)
{
	/* assuming that the onsetAnalysisMatrix was derived from the above onsetAnalysis,
	 (which is beyond likely in this codebase because it's not so trivial to construct that array2dReal),
	 the sample rate of the signal will have already been converted to 44100 before the analysis.
	 */

	constexpr float frameRate = 44100.f / 512.f;

	essentia::standard::Algorithm* onsetDetectionSeconds = factory.create (
		"Onsets",
		  "frameRate",       frameRate,
		  "silenceThreshold",settings.onset.silenceThreshold,
		  "alpha",           settings.onset.alpha, // proportion of the mean included to reject smaller peaks-filters very short onsets
		  "delay",           settings.onset.numFrames_shortOnsetFilter // number of frames used to compute the threshold-size of short-onset filter
	);

    const vecReal weights = getWeights(settings);
	
	vecReal onsets;
	onsetDetectionSeconds->input("detections").set(onsetAnalysisMatrix);
	onsetDetectionSeconds->input("weights").set(weights);
	onsetDetectionSeconds->output("onsets").set(onsets);
	
	onsetDetectionSeconds->compute();
	
	for (size_t i = 1; i < onsets.size(); ++i) {
		assert(onsets[i - 1] < onsets[i]);
	}
	
	return onsets;
}

vecVecReal featuresForSbic(const vecReal &waveform,
						   const AlgorithmFactory &factory,
						   const AnalyzerSettings &settings,
						   RunLoopStatus& rls,
						   const ShouldExitFn &shouldExit)
{
	auto *inVec = new vectorInput(&waveform);

    const auto sr = static_cast<float>(settings.analysis.sampleRate);
	assert (0.0 < sr);
	int const frameSize = settings.analysis.frameSize;
	int const hopSize = settings.analysis.hopSize;

    constexpr float validFrameThresholdRatio = 0.f;
	
	int const zeroPadding = frameSize;
	
	int const fftSize = frameSize * 2;
	
	
	Algorithm* frameCutter = factory.create ("FrameCutter",
		"frameSize",               frameSize,
		"hopSize",                 hopSize,
		"lastFrameToEndOfFile",    true,
		"silentFrames",            std::string ("keep"),
		"startFromZero",           true,
		"validFrameThresholdRatio", validFrameThresholdRatio
	);
	
	Algorithm* windowing = factory.create ("Windowing",
		"normalized", false,
		"size",        frameSize,
		"zeroPadding", zeroPadding,
		"type",        settings.analysis.windowingType.toStdString(),
		"zeroPhase",   false
	);
	Algorithm* spectrum = factory.create ("PowerSpectrum",
		"size", fftSize
	);
	Algorithm* bfcc = factory.create ("BFCC",
		"sampleRate",           sr,
		"dctType",              settings.bfcc.dctType.toStdString(),
		"highFrequencyBound",   settings.bfcc.highFrequencyBound,
		"lowFrequencyBound",    settings.bfcc.lowFrequencyBound,
		"inputSize",            frameSize + 1,
		"liftering",            settings.bfcc.liftering,
		"normalize",            settings.bfcc.normalize.toStdString(),
		"numberBands",          settings.bfcc.numBands,
		"numberCoefficients",   settings.bfcc.numCoefficients,
		"weighting",            settings.bfcc.weightingType.toStdString(),
		"type",                 settings.bfcc.spectrumType.toStdString(),
		"logType",              "dbpow"	// log compr. type. Use ‘dbpow’ if working with power and ‘dbamp’ if working with magnitudes. DONT CHANGE unless also changing PowerSpectrum algo to Spectrum
	);
	Algorithm* barkFrameAccumulator = factory.create("VectorRealAccumulator");
	Algorithm* bfccFrameAccumulator = factory.create("VectorRealAccumulator");
	
	// for some reason, to get this working in as a connection to FrameAccumulator,
	// these must be vector<vector<vector<Real>>>, and the 1st dimension only has size of 1...
	std::vector<std::vector<vecReal>> barkBands, BFCCs;
	VectorOutput<std::vector<vecReal>> *barkAccumOutput = new VectorOutput<std::vector<vecReal>>(&barkBands);
	VectorOutput<std::vector<vecReal>> *bfccAccumOutput = new VectorOutput<std::vector<vecReal>>(&BFCCs);
	
	*inVec								>>	frameCutter->input("signal");
	frameCutter->output("frame")		>>	windowing->input("frame");
	windowing->output("frame")			>>	spectrum->input("signal");
	spectrum->output("powerSpectrum")	>>	bfcc->input("spectrum");
	bfcc->output("bands")				>>	barkFrameAccumulator->input("data");
	bfcc->output("bfcc")				>>	bfccFrameAccumulator->input("data");
	barkFrameAccumulator->output("array") >> *barkAccumOutput;
	bfccFrameAccumulator->output("array") >> *bfccAccumOutput;
	
	Network n(inVec);
	n.runPrepare();
	while (n.runStep()){
		if (shouldExit()){
			break;
		}
	}
	n.clear();
	
	assert(BFCCs.size() == 1);
	assert(BFCCs[0].size());
	assert(BFCCs[0][0].size());

	return BFCCs[0];	// the only dimension that was used
}

vecReal sBic(const array2dReal &featureMatrix, const standardFactory &factory,
			 const AnalyzerSettings &settings){

	standard::Algorithm* sbic = factory.create (
		"SBic",
		  "cpw",       settings.sBic.complexityPenaltyWeight,
		  "inc1",      settings.sBic.incrementFirstPass,
		  "inc2",      settings.sBic.incrementSecondPass,
		  "minLength", settings.sBic.minSegmentLengthFrames,
		  "size1",     settings.sBic.sizeFirstPass,
		  "size2",     settings.sBic.sizeSecondPass
	);
	// "cpw" 1.5, "inc1" 60, "inc2" 20, "minLength" 10, "size1" 300, size2" 200
	vecReal segmentationVec;
	sbic->input("features").set(featureMatrix);
	sbic->output("segmentation").set(segmentationVec);
	sbic->compute();
	return segmentationVec;
}

vecVecReal splitWaveIntoEvents(const vecReal&wave, const vecReal&onsetsInSeconds,
							   const streamingFactory &factory,
							   const AnalyzerSettings &settings,
							   RunLoopStatus& rls, const ShouldExitFn &shouldExit){
	size_t const numOnsets {onsetsInSeconds.size()};
	assert(numOnsets);
	if (numOnsets == 1){	// only 1 event
		vecVecReal retVal{wave};
		return retVal;
	}
	vecReal endTimes(numOnsets);
	std::copy(onsetsInSeconds.begin() + 1, onsetsInSeconds.end(), endTimes.begin());
	assert(onsetsInSeconds[1] == endTimes[0]);
	
    const auto sampleRate = static_cast<float>(settings.analysis.sampleRate);
	assert (sampleRate > 22000.f);
	
	Real const endOfFile = static_cast<Real>((wave.size() - 1)) / sampleRate;
	Real const a = onsetsInSeconds.back();
	assert (a < endOfFile);
	endTimes.back() = endOfFile;
	assert(*(onsetsInSeconds.end() - 1) == *(endTimes.end() - 2));
	
	Algorithm* slicer = factory.create("Slicer",
									   "timeUnits", "seconds",
									   "sampleRate", sampleRate,
									   "startTimes", onsetsInSeconds,
									   "endTimes", endTimes);
	
	auto *waveInput = new vectorInput(&wave);
	vecVecReal waveEvents;
	vectorOutputCumulative *waveEventsOutput = new vectorOutputCumulative(&waveEvents);
	
	*waveInput >> slicer->input("audio");
	slicer->output("frame") >> *waveEventsOutput;
	
	Network n(waveInput);
	n.runPrepare();
	while (n.runStep()){
		if (shouldExit()){
			break;
		}
	}
	n.clear();
	
	assert(!waveEvents.empty());
	
	for (auto & waveEvent : waveEvents){
		size_t currentLength = waveEvent.size();
		const size_t fadeInSamps = std::min(static_cast<size_t>(settings.split.fadeInSamps), currentLength);
		for (size_t j = 0; j < fadeInSamps; ++j){
			waveEvent[j] = waveEvent[j] * (static_cast<Real>(j) / static_cast<Real>(fadeInSamps));
		}
		size_t fadeOutSamps = std::min(static_cast<size_t>(settings.split.fadeOutSamps), currentLength);
		for (size_t j = 0; j < fadeOutSamps; ++j){
			const size_t currentIdx = (currentLength - 1) - j;
			waveEvent[currentIdx] = waveEvent[currentIdx] * (static_cast<Real>(j) / static_cast<Real>(fadeOutSamps));
		}
	}
	
	return waveEvents;
}

void writeWav(const vecReal&wave, const std::string_view name, const streamingFactory &factory,
			  const AnalyzerSettings &settings,
			  RunLoopStatus& rls,
			  const ShouldExitFn &shouldExit)
{
    const auto sr = static_cast<float>(settings.analysis.sampleRate);
	jassert (sr > 20000.f);
	Algorithm* writer = factory.create("MonoWriter",
									   "filename", std::string(name) + ".wav",
									   "format", "wav",
									   "sampleRate", sr);
	vectorInput *waveInput = new vectorInput(&wave);
	*waveInput >> writer->input("audio");
	
	Network n(waveInput);
	n.runPrepare();
	while (n.runStep()){
		if (shouldExit()){
			break;
		}
	}
	n.clear();
}
void writeWavs(const vecVecReal &waves, const std::string_view defName, const streamingFactory &factory,
			   const AnalyzerSettings &settings,
			   RunLoopStatus& rls,
			   const ShouldExitFn &shouldExit)
{
	int idx = 0;
	std::string name(defName);
    for (auto const &wave : waves){
		std::string strIdx = std::to_string(idx++);
		name += strIdx;					// add index to name
		
		writeWav(wave, name, factory, settings, rls, shouldExit);
		
		name.erase(name.back() - strIdx.length(), name.back());	// remove index from name
	}
}

} // namespace nvs::analysis

