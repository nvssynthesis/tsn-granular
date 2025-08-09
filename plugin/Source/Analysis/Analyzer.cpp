/*
  ==============================================================================

    Analyzer.cpp
    Created: 27 Sep 2023 12:25:12am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analysis/Analyzer.h"
#include <numeric>
#include <concepts>

namespace nvs {
namespace analysis {

Analyzer::Analyzer()
:	ess_init()
,	ess_hold(ess_init)
{}

bool Analyzer::updateSettings(juce::ValueTree newSettings){
	// verify tree structure
	bool const valid = verifySettingsStructure(newSettings);
	jassert (newSettings.getParent().getChildWithName("FileInfo").hasProperty("sampleRate"));
	
	if (valid){
		updateSettingsFromValueTree(settings, newSettings);
	}
	else {
		std::cerr << "settings tree invalid\n";
		jassertfalse;
	}
	return valid;
}
AnalyzerSettings const &Analyzer::getSettings() const {
	return settings;
}

float Analyzer::getAnalyzedFileSampleRate() const {
	return settings.analysis.sampleRate;
}

std::optional<vecReal> Analyzer::calculateOnsetsInSeconds(vecReal const &wave, RunLoopStatus& rls, ShouldExitFn shouldExit){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = calculateOnsetsMatrix(wave, ess_hold.factory, settings, rls, shouldExit);
	std::cout << "analyzed onsets\n";
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")
	
	std::vector<float> onsetsInSeconds = analysis::calculateOnsetsInSeconds(onsets2d, tmpStFac, settings);	// needs namespace qualifier to not call itself
	std::cout << "calculated onsets in seconds\n";

	return onsetsInSeconds;
}

EventwisePitchDescription Analyzer::calculateEventwisePitchDescription(vecReal const &waveEvent) {
	auto const p_tmp = calculatePitchesAndConfidences(waveEvent, ess_hold.factory, settings);
	auto const p_mean = mean(p_tmp.pitches);
	
	EventwisePitchDescription descr {
		.mean = p_mean,
		.median = essentia::median(p_tmp.pitches),
		.variance = essentia::variance(p_tmp.pitches, p_mean),
		.skewness = essentia::skewness(p_tmp.pitches, p_mean),
		.kurtosis = essentia::kurtosis(p_tmp.pitches, p_mean)
	};
	return descr;
}

EventwiseStatistics<float> Analyzer::calculateEventwiseLoudness(vecReal const &waveEvent){
	vecReal l_tmp = calculateLoudnesses(waveEvent, ess_hold.factory, settings);
	
	auto const l_mean = mean(l_tmp);
	
	EventwisePitchDescription descr {
		.mean = l_mean,
		.median = essentia::median(l_tmp),
		.variance = essentia::variance(l_tmp, l_mean),
		.skewness = essentia::skewness(l_tmp, l_mean),
		.kurtosis = essentia::kurtosis(l_tmp, l_mean)
	};
	return descr;
}

EventwiseBFCCDescription Analyzer::calculateEventwiseBFCCDescription(vecReal  const &waveEvent) {
	vecVecReal b_tmp = calculateBFCCs(waveEvent, ess_hold.factory, settings);	// bfccs per frame
	
	vecReal const means = essentia::meanFrames(b_tmp);	// get mean per bfcc across all frames
	vecReal const  medians = essentia::medianFrames(b_tmp);
	vecReal const  variances = essentia::varianceFrames(b_tmp);
	vecReal const  skewnesses = essentia::skewnessFrames(b_tmp);
	vecReal const  kurtoses = essentia::kurtosisFrames(b_tmp);
	
	size_t N = b_tmp[0].size();
	std::vector<EventwiseStatistics<Real>> descriptions;
	descriptions.reserve(N);
	
	for (size_t i = 0; i < N; ++i) {
		descriptions.push_back({
			.mean 	   = means[i],
			.median    = medians[i],
			.variance  = variances[i],
			.skewness  = skewnesses[i],
			.kurtosis  = kurtoses[i]
		});
	}

	return descriptions;
}

std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>>
Analyzer::calculateOnsetwiseTimbreSpace(vecReal const &wave, std::vector<float> const &onsetsInSeconds, RunLoopStatus& rls, ShouldExitFn shouldExit){
	if ((!wave.size()) || (!onsetsInSeconds.size())){
		return std::nullopt;
	}
	
	const double startMs = juce::Time::getMillisecondCounterHiRes();
	auto   startTimeStr = juce::Time::getCurrentTime().toString (true, true, true, true);
	std::cout << "calculateOnsetwiseTimbreSpace start: " << startTimeStr << "\n";
	
	rls.set("Splitting Wave into Events...");
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, settings, rls, shouldExit);
#pragma message("probably need some normalization, possibly based on variance")
	
	std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbre_points;
	
	rls.set("Calculating timbre descriptions per event...");
	for (size_t i = 0; i < events.size(); ++i) {
		auto const &e = events[i];
		FeatureContainer<EventwiseStatistics<Real>> f;
		
		f.bfccs = calculateEventwiseBFCCDescription(e);
		f.f0 = calculateEventwisePitchDescription(e);
		f.loudness = calculateEventwiseLoudness(e);
		
		timbre_points.push_back(f);

		std::cout << "got timbre description #" << i << "/" << events.size() << "\n";
		
		rls.set((double)i / (double)events.size());
	}
	std::cout << "calculated all BFCCs\n";
	
	const double endMs   = juce::Time::getMillisecondCounterHiRes();
	auto   endTimeStr   = juce::Time::getCurrentTime().toString (true, true);
	const double elapsed = (endMs - startMs) * 0.001f;  // in seconds

	std::cout << "calculateOnsetwiseTimbreSpace end:   " << endTimeStr << "\n";
	std::cout << "\t\t\t Elapsed time:   " << juce::String (elapsed, 3) << " seconds\n";
	
	return timbre_points;
}

std::optional<vecVecReal> Analyzer::calculatePCA(std::vector<FeatureContainer<EventwiseStatistics<Real>>> const &allFeatures, std::vector<Features> featuresToUse, Statistic statToUse) {
	if (allFeatures.size() < 2){	// can't perform PCA with 1 sample
		return std::nullopt;
	}
	// gather desired features into vecVecReal
	vecVecReal V;
	V.reserve(allFeatures.size());
	
	for (auto const &f : allFeatures){
		V.push_back(extractFeatures(f, featuresToUse, Statistic::Median));
	}
	
	vecVecReal pca = nvs::analysis::PCA(V, ess_hold.standardFactory, 6);
	std::cout << "calculated PCAs\n";
	return pca;
}

vecVecReal truncate(vecVecReal const &V, size_t trunc){
	if (V.size() < trunc){
		return V;
	}
	vecVecReal Vtrunc(trunc);
	for (size_t i = 0; i < trunc; ++i){
		Vtrunc[i] = V[i];
	}
	return Vtrunc;
};

vecVecReal transpose(vecVecReal const &V){
	size_t const D0 = V.size();
	size_t const D1 = V[0].size();
	for (auto const &v : V){
		assert (v.size() == D1);
	}
	
	vecVecReal Vtranspose(D1);
	for (size_t i = 0; i < D1; ++i){
		Vtranspose[i].resize(D0);
		for (size_t j = 0; j < D0; ++j){
			Vtranspose[i][j] = V[j][i];
		}
	}
	return Vtranspose;
}


void writeEventsToWav(vecReal const &wave, std::vector<float> const &onsetsInSeconds, std::string_view ogPath, Analyzer &analyzer, RunLoopStatus& rls, ShouldExitFn shouldExit)
{
	if ( !wave.size() or !onsetsInSeconds.size() ){
		std::cerr << "unsuccessful write; wave or onsets of size 0\n";
		return;
	}
	juce::String const base_name(ogPath.data());
	base_name.dropLastCharacters(4);
	
	auto settings = analyzer.getSettings();
//	denormalizeOnsets(normalizedOnsets, getLengthInSeconds(wave.size(), analyzer._analysisSettings.sampleRate));
//	auto const &onsetsInSeconds = normalizedOnsets;	// merely renaming because now normalizedOnsets are in fact not normalized; they are in seconds.
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds,
														   analyzer.ess_hold.factory, settings, rls, shouldExit);
	juce::WavAudioFormat format;
	std::unique_ptr<juce::AudioFormatWriter> writer;

	int idx = 0;
	for (auto const &e : events){
		juce::String evName = base_name;
		evName.append("_", 2);
		evName.append(std::to_string(idx++), 4);
		evName.append(".wav", 4);
		juce::File outFile(evName);
		
		juce::AudioBuffer<float> buffer(1, static_cast<int>(e.size()));
		buffer.clear();
		buffer.addFrom(0, 0, e.data(), static_cast<int>(e.size()));
//			buffer.applyGainRamp(0, 0, 									5, 0.f, 1.f);	// fade in 5 samps
//			buffer.applyGainRamp(0, static_cast<int>(e.size())-10, 10, 1.f, 0.f);	// fade out 10 samps
		
		writer.reset (format.createWriterFor (new juce::FileOutputStream (outFile),	// OutputStream* streamToWriteTo,
											  analyzer.getAnalyzedFileSampleRate(), // double sampleRateToUse
											  buffer.getNumChannels(),		//	unsigned int numberOfChannels
											  24,							//	int bitsPerSample
											  {},							//	const StringPairArray& metadataValues
											  0));							//	int qualityOptionIndex
		if (writer != nullptr){
			writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
			std::cout << "File " << outFile.getFileName() << " written\n";
		} else {
			std::cerr << "File write " << outFile.getFileName() << " fail\n";
		}
	}
}

}	// namespace analysis
}	// namespace nvs

