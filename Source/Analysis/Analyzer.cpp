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
,	settingsTree("Settings")
{
	initializeSettingsBranches(settingsTree);
}

void Analyzer::updateSettings(juce::ValueTree newSettings){
	// verify tree structure
	
	settingsTree = newSettings;
}
juce::ValueTree &Analyzer::getSettings() {
	return settingsTree;
}

void Analyzer::setAnalyzedFileSampleRate(float sampleRate){
	auto s = settingsTree.getParent().toXmlString();
	std::cout << "Set analyzed sr, parent tree is originally: " << s << "\n";
	settingsTree.getParent().getChildWithName("PresetInfo").setProperty("sampleRate", sampleRate, nullptr);
}
float Analyzer::getAnalyzedFileSampleRate() const{
	auto s = settingsTree.getParent().toXmlString();
	std::cout << "Get analyzed sr, parent tree is: " << s << "\n";
	return settingsTree.getParent().getChildWithName("PresetInfo").getProperty("sampleRate");
}

std::optional<vecReal> Analyzer::calculateOnsetsInSeconds(vecReal const &wave, std::function<bool(void)> runLoopCallback){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = calculateOnsetsMatrix(wave, ess_hold.factory, settingsTree, runLoopCallback);
	std::cout << "analyzed onsets\n";
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")
	
	std::vector<float> onsetsInSeconds = analysis::calculateOnsetsInSeconds(onsets2d, tmpStFac, settingsTree);	// needs namespace qualifier to not call itself
	std::cout << "calculated onsets in seconds\n";

	
	
	return onsetsInSeconds;
}

EventwisePitchDescription Analyzer::calculateEventwisePitchDescription(vecReal const &waveEvent) {
	auto const p_tmp = calculatePitchesAndConfidences(waveEvent, ess_hold.factory, settingsTree);
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

#pragma message("doesnt work")
EventwiseBFCCDescription Analyzer::calculateEventwiseBFCCDescription(vecReal  const &waveEvent) {
	std::cout << "calc bfcc descr tree: " << settingsTree.toXmlString() << "\n";
	vecVecReal b_tmp = calculateBFCCs(waveEvent, ess_hold.factory, settingsTree);	// bfccs per frame
	
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
Analyzer::calculateOnsetwiseTimbreSpace(vecReal const &wave, std::vector<float> const &onsetsInSeconds){
	if ((!wave.size()) || (!onsetsInSeconds.size())){
		return std::nullopt;
	}
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, settingsTree);
#pragma message("probably need some normalization, possibly based on variance")
	
	std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbre_points;
	
	for (vecReal const &e : events){
		FeatureContainer<EventwiseStatistics<Real>> f;
		
		f.bfccs = calculateEventwiseBFCCDescription(e);
		f.f0 = calculateEventwisePitchDescription(e);
		
		timbre_points.push_back(f);

		std::cout << "got BFCCs for event\n";
	}
	std::cout << "calculated all BFCCs\n";
	return timbre_points;
}

std::optional<vecVecReal> Analyzer::calculatePCA(std::vector<FeatureContainer<EventwiseStatistics<Real>>> const &allFeatures, std::set<Features> featuresToUse, Statistic statToUse) {
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
	for (int i = 0; i < trunc; ++i){
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
	for (int i = 0; i < D1; ++i){
		Vtranspose[i].resize(D0);
		for (int j = 0; j < D0; ++j){
			Vtranspose[i][j] = V[j][i];
		}
	}
	return Vtranspose;
}


void writeEventsToWav(vecReal const &wave, std::vector<float> const &onsetsInSeconds, std::string_view ogPath, Analyzer &analyzer)
{
	if ( !wave.size() | !onsetsInSeconds.size() ){
		std::cerr << "unsuccessful write; wave or onsets of size 0\n";
		return;
	}
	juce::String const base_name(ogPath.data());
	base_name.dropLastCharacters(4);
	
	auto settingsTree = analyzer.getSettings();
//	denormalizeOnsets(normalizedOnsets, getLengthInSeconds(wave.size(), analyzer._analysisSettings.sampleRate));
//	auto const &onsetsInSeconds = normalizedOnsets;	// merely renaming because now normalizedOnsets are in fact not normalized; they are in seconds.
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds,
														   analyzer.ess_hold.factory, settingsTree);
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

