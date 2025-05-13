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

namespace nvs {
namespace analysis {

vecReal makeSweptSine(Real const low, Real const high, size_t const len, Real const sampleRate){
	std::vector<Real> freqSweep(len, 0.f);
	for (size_t i = 0; i < len; ++i){
		Real x = (high - low) * (Real(i)/Real(len)) + low;
		freqSweep[i] = std::sin(2.f * 3.14159265f * (x / sampleRate));
	}
	
	return freqSweep;
}

array2dReal calculateOnsetsMatrix(std::vector<Real> const &waveform,
						  streamingFactory const &factory, juce::ValueTree settingsTree,
						  RunLoopStatus& rls, ShouldExitFn shouldExit)
{
	auto presetInfoTree = settingsTree.getParent().getChildWithName("PresetInfo");
	auto const input_sr     = (float) presetInfoTree.getProperty ("sampleRate");
	auto analysisTree   = settingsTree.getChildWithName ("Analysis");
	auto const internal_sr  = 44100.0f;
	auto const framesize    = (int)   analysisTree.getProperty ("frameSize");
	auto const hopsize      = (int)   analysisTree.getProperty ("hopSize");


	vectorInput *inVec = new vectorInput(&waveform);
	
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
	Algorithm* frameCutter  = factory.create("FrameCutter",
											 "frameSize", framesize,
											 "hopSize", hopsize,
											 "startFromZero", false,
											 "lastFrameToEndOfFile", true,	// irrelevant unless startFromZero is true
											 "validFrameThresholdRatio", 0.0f);	// at 1, only frames with length frameSize are kept
	
	Algorithm* windowingToFFT = factory.create("Windowing",
											 "normalized", true,
											 "size", framesize,
											 "zeroPhase", false,
											 "zeroPadding", framesize ,	/*
																		 zero padding as such resolves potential for
																		 "essentia::EssentiaException: TriangularBands: the number of spectrum bins is insufficient for the specified number of triangular bands. Use zero padding to increase the number of FFT bins."
																		 */
											 "type", "hamming");	// options: hamming, hann, hannnsgcq, triangular, square, blackmanharris62, blackmanharris70, blackmanharris74, blackmanharris92
	
	Algorithm* FFT			= factory.create("FFT",
											 "size", framesize);
	
	Algorithm* carToPol		= factory.create("CartesianToPolar");
	
	Algorithm* onsetDetectionHfc = factory.create("OnsetDetection",
											"method", "hfc",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	Algorithm* onsetDetectionComplex = factory.create("OnsetDetection",
											"method", "complex",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	Algorithm* onsetDetectionComplexPhase = factory.create("OnsetDetection",
											"method", "complex_phase",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	Algorithm* onsetDetectionFlux = factory.create("OnsetDetection",
											"method", "flux",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	Algorithm* onsetDetectionMelFlux = factory.create("OnsetDetection",
											"method", "melflux",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	Algorithm* onsetDetectionRms = factory.create("OnsetDetection",
											"method", "rms",	// options: hfc, complex, complex_phase, flux, melflux, rms
											   "sampleRate", internal_sr);
	vecReal onsetDetVecHFC;
	vectorOutput *onsetDetsHFC = new vectorOutput(&onsetDetVecHFC);
	vecReal onsetDetVecComplex;
	vectorOutput *onsetDetsComplex = new vectorOutput(&onsetDetVecComplex);
	vecReal onsetDetVecComplexPhase;
	vectorOutput *onsetDetsComplexPhase = new vectorOutput(&onsetDetVecComplexPhase);
	vecReal onsetDetVecFlux;
	vectorOutput *onsetDetsFlux = new vectorOutput(&onsetDetVecFlux);
	vecReal onsetDetVecMelFlux;
	vectorOutput *onsetDetsMelFlux = new vectorOutput(&onsetDetVecMelFlux);
	vecReal onsetDetVecRms;
	vectorOutput *onsetDetsRms = new vectorOutput(&onsetDetVecRms);
	
	*inVec >> resampler->input("signal");
	resampler->output("signal") >> frameCutter->input("signal");
	frameCutter->output("frame") >> windowingToFFT->input("frame");
	windowingToFFT->output("frame") >> FFT->input("frame");
	FFT->output("fft") >> carToPol->input("complex");
	
	carToPol->output("magnitude") >> onsetDetectionHfc->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionHfc->input("phase");
	
	carToPol->output("magnitude") >> onsetDetectionComplex->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionComplex->input("phase");
	
	carToPol->output("magnitude") >> onsetDetectionComplexPhase->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionComplexPhase->input("phase");
	
	carToPol->output("magnitude") >> onsetDetectionFlux->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionFlux->input("phase");
	
	carToPol->output("magnitude") >> onsetDetectionMelFlux->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionMelFlux->input("phase");
	
	carToPol->output("magnitude") >> onsetDetectionRms->input("spectrum");
	carToPol->output("phase")	>> onsetDetectionRms->input("phase");
	
	onsetDetectionHfc->output("onsetDetection") >> *onsetDetsHFC;
	onsetDetectionComplex->output("onsetDetection") >> *onsetDetsComplex;
	onsetDetectionComplexPhase->output("onsetDetection") >> *onsetDetsComplexPhase;
	onsetDetectionFlux->output("onsetDetection") >> *onsetDetsFlux;
	onsetDetectionMelFlux->output("onsetDetection") >> *onsetDetsMelFlux;
	onsetDetectionRms->output("onsetDetection") >> *onsetDetsRms;

	assert(onsetDetVecHFC.size() == onsetDetVecComplex.size());
	
	Network n(inVec);
	n.runPrepare();
	rls.set("Computing onset matrix...");
	rls.set(0.0);
	while (n.runStep()){
		if (shouldExit()) {
			break;
		}
	}
	rls.set(1.0);
	n.clear();
	
	TNT::Array2D<essentia::Real> onsetsMatrix(6, static_cast<int>(onsetDetVecHFC.size()));
	for (size_t j = 0; j < onsetDetVecHFC.size(); ++j){
		onsetsMatrix[0][j] = onsetDetVecHFC[j];
		onsetsMatrix[1][j] = onsetDetVecComplex[j];
		onsetsMatrix[2][j] = onsetDetVecComplexPhase[j];
		onsetsMatrix[3][j] = onsetDetVecFlux[j];
		onsetsMatrix[4][j] = onsetDetVecMelFlux[j];
		onsetsMatrix[5][j] = onsetDetVecRms[j];
	}
	return onsetsMatrix;
}

std::vector<juce::String> getOnsetWeightKeys()
{
	std::vector<juce::String> keys;
	keys.reserve (onsetSpecs.size());

	for (auto& kv : onsetSpecs)
	{
		const auto& key = kv.first;
		if (key.startsWith ("weight_"))
			keys.push_back (key);
	}

	// Optional: sort them if you want a consistent order
	std::sort (keys.begin(), keys.end());
	return keys;
}

#pragma message("make this work with StreamingFactory")
vecReal calculateOnsetsInSeconds(array2dReal onsetAnalysisMatrix, standardFactory const &factory, juce::ValueTree settingsTree)
{
	/* assuming that the onsetAnalysisMatrix was derived from the above onsetAnalysis,
	 (which is beyond likely in this codebase because it's not so trivial to construct that array2dReal),
	 the sample rate of the signal will have already been converted to 44100 before the analysis.
	 */
	auto analysisTree = settingsTree.getChildWithName ("Analysis");
	auto onsetTree    = settingsTree.getChildWithName ("Onset");

	float frameRate = 44100.f / (float) analysisTree.getProperty ("hopSize");

	essentia::standard::Algorithm* onsetDetectionSeconds = factory.create (
		"Onsets",
		  "frameRate",       frameRate,
		  "silenceThreshold",(float) onsetTree.getProperty ("silenceThreshold"),
		  "alpha",           (float) onsetTree.getProperty ("alpha"), // proportion of the mean included to reject smaller peaks-filters very short onsets
		  "delay",           (int)   onsetTree.getProperty ("numFrames_shortOnsetFilter") // number of frames used to compute the threshold-size of short-onset filter
	);

	vecReal weights;
	auto weightKeys = getOnsetWeightKeys();
	weights.reserve (weightKeys.size());
	for (auto& key : weightKeys) {
		weights.push_back ((float) onsetTree.getProperty (key));
	}
	
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

vecVecReal featuresForSbic(vecReal const &waveform,
						   AlgorithmFactory const &factory,
						   juce::ValueTree settingsTree,
						   RunLoopStatus& rls, ShouldExitFn shouldExit)
{
	vectorInput *inVec = new vectorInput(&waveform);
	
	auto presetInfoTree = settingsTree.getParent().getChildWithName("PresetInfo");
	auto analysisTree = settingsTree.getChildWithName ("Analysis");
	auto bfccTree     = settingsTree.getChildWithName ("BFCC");
	
	Algorithm* frameCutter = factory.create ("FrameCutter",
		"frameSize",               (int)    analysisTree.getProperty ("frameSize"),
		"hopSize",                 (int)    analysisTree.getProperty ("hopSize"),
		"lastFrameToEndOfFile",    true,
		"silentFrames",            std::string ("keep"),
		"startFromZero",           true,
		"validFrameThresholdRatio",(float) analysisTree.getProperty ("validFrameThresholdRatio", 0.0)
	);
	
	Algorithm* windowing = factory.create ("Windowing",
		"normalized", false,
		"size",        (int)    analysisTree.getProperty ("frameSize"),
		"zeroPadding", (int)    analysisTree.getProperty ("frameSize"),
		"type",        bfccTree.getProperty ("windowingType", "hann").toString().toStdString(),
		"zeroPhase",   false
	);
	Algorithm* spectrum = factory.create ("PowerSpectrum",
		"size", (int) analysisTree.getProperty ("frameSize") * 2
	);
	Algorithm* bfcc = factory.create ("BFCC",
		"sampleRate",           (float) presetInfoTree.getProperty ("sampleRate",    44100.0),
		"dctType",              (int)    bfccTree.getProperty ("dctType",       2),
		"highFrequencyBound",   (float) bfccTree.getProperty ("highFrequencyBound", 11000.0),
		"lowFrequencyBound",    (float) bfccTree.getProperty ("lowFrequencyBound",   20.0),
		"inputSize",            (int)    analysisTree.getProperty ("frameSize") + 1,
		"liftering",            (int)    bfccTree.getProperty ("liftering",    0),
		"normalize",            bfccTree.getProperty("normalize", "unit_sum").toString().toStdString(),
		"numberBands",          (int)    bfccTree.getProperty ("numBands",      40),
		"numberCoefficients",   (int)    bfccTree.getProperty ("numCoefficients", 13),
		"weighting",            bfccTree.getProperty("weightingType","linear").toString().toStdString(),
		"type",                 "power",
		"logType",              "dbpow"
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

vecReal sBic(array2dReal featureMatrix, standardFactory const &factory, juce::ValueTree settingsTree){
	auto sBicTree = settingsTree.getChildWithName ("sBic");

	standard::Algorithm* sbic = factory.create (
		"SBic",
		  "cpw",       (float) sBicTree.getProperty ("complexityPenaltyWeight"),
		  "inc1",      (int)    sBicTree.getProperty ("incrementFirstPass"),
		  "inc2",      (int)    sBicTree.getProperty ("incrementSecondPass"),
		  "minLength", (int)    sBicTree.getProperty ("minSegmentLengthFrames"),
		  "size1",     (int)    sBicTree.getProperty ("sizeFirstPass"),
		  "size2",     (int)    sBicTree.getProperty ("sizeSecondPass")
	);
	// "cpw" 1.5, "inc1" 60, "inc2" 20, "minLength" 10, "size1" 300, size2" 200
	vecReal segmentationVec;
	sbic->input("features").set(featureMatrix);
	sbic->output("segmentation").set(segmentationVec);
	sbic->compute();
	return segmentationVec;
}

vecVecReal splitWaveIntoEvents(vecReal const &wave, vecReal const &onsetsInSeconds,
							   streamingFactory const &factory,
							   juce::ValueTree settingsTree,
							   RunLoopStatus& rls, ShouldExitFn shouldExit){
	size_t const numOnsets {onsetsInSeconds.size()};
	assert(numOnsets);
	if (numOnsets == 1){	// only 1 event
		vecVecReal retVal{wave};
		return retVal;
	}
	vecReal endTimes(numOnsets);
	std::copy(onsetsInSeconds.begin() + 1, onsetsInSeconds.end(), endTimes.begin());
	assert(onsetsInSeconds[1] == endTimes[0]);
	
	float sampleRate = settingsTree.getParent().getChildWithName("PresetInfo").getProperty ("sampleRate");
	assert (sampleRate > 22000.f);
	
	Real endOfFile = static_cast<Real>((wave.size() - 1)) / sampleRate;
	endTimes.back() = endOfFile;
	assert(*(onsetsInSeconds.end() - 1) == *(endTimes.end() - 2));

	Algorithm* slicer = factory.create("Slicer",
									   "timeUnits", "seconds",
									   "sampleRate", sampleRate,
									   "startTimes", onsetsInSeconds,
									   "endTimes", endTimes);
	
	vectorInput *waveInput = new vectorInput(&wave);
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
	
	assert(waveEvents.size());
	
	auto splitSettings = settingsTree.getChildWithName("Split");
	
	for (size_t i = 0; i < waveEvents.size(); ++i){
		size_t currentLength = waveEvents[i].size();
		size_t fadeInSamps = std::min(size_t((int)splitSettings.getProperty("fadeInSamps")), currentLength);
		for (size_t j = 0; j < fadeInSamps; ++j){
			waveEvents[i][j] = waveEvents[i][j] * ((Real)j / (Real)fadeInSamps);
		}
		size_t fadeOutSamps = std::min(size_t((int)splitSettings.getProperty("fadeOutSamps")), currentLength);
		for (size_t j = 0; j < fadeOutSamps; ++j){
			size_t currentIdx = (currentLength - 1) - j;
			waveEvents[i][currentIdx] = waveEvents[i][currentIdx] * ((Real)j / (Real)fadeOutSamps);
		}
	}
	
	return waveEvents;
}

void writeWav(vecReal const &wave, std::string_view name, streamingFactory const &factory,
			  juce::ValueTree settingsTree,
			  RunLoopStatus& rls, ShouldExitFn shouldExit)
{
	float sr = (float)settingsTree.getParent().getChildWithName("PresetInfo").getProperty ("sampleRate");
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
void writeWavs(vecVecReal const &waves, std::string_view defName, streamingFactory const &factory,
			   juce::ValueTree settingsTree, RunLoopStatus& rls, ShouldExitFn shouldExit)
{
	int idx = 0;
	std::string name(defName);
	std::string strIdx;
	for (auto const &wave : waves){
		strIdx = std::to_string(idx);
		name += strIdx;					// add index to name
		
		writeWav(wave, name, factory, settingsTree, rls, shouldExit);
		
		name.erase(name.back() - strIdx.length(), name.back());	// remove index from name
	}
}

}	// namespace analysis
}	// namespace nvs
