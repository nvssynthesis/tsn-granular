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
:	ess_hold(ess_init)
{}


std::optional<vecReal> Analyzer::calculateOnsetsInSeconds(vecReal const &wave, std::function<bool(void)> runLoopCallback){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = calculateOnsetsMatrix(wave, ess_hold.factory, _analysisSettings, runLoopCallback);
	std::cout << "analyzed onsets\n";
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")
	
	std::vector<float> onsetsInSeconds = analysis::calculateOnsetsInSeconds(onsets2d, tmpStFac, _analysisSettings, _onsetSettings);	// needs namespace qualifier to not call itself
	std::cout << "calculated onsets in seconds\n";

	return onsetsInSeconds;
}

EventwisePitchDescription Analyzer::calculateEventwisePitchDescription(vecReal const &waveEvent) {
	auto p_tmp = calculatePitchesAndConfidences(waveEvent, ess_hold.factory, _analysisSettings, _pitchSettings);
	
	EventwisePitchDescription descr {
		.median = essentia::median(p_tmp.pitches),
		.range{},
		.slope{}
	};
	return descr;
}

EventwiseBFCCDescription Analyzer::calculateEventwiseBFCCDescription(vecReal  const &waveEvent) {
	vecVecReal b_tmp = calculateBFCCs(waveEvent, ess_hold.factory, _analysisSettings, _bfccSettings);
	
	EventwiseBFCCDescription descr {
		.median {binwiseStatistic(b_tmp, essentia::median<Real>)},
		.range {},
		.slope {}
	};
	return descr;
}

std::optional<vecVecReal> Analyzer::calculateOnsetwiseTimbreSpace(vecReal const &wave, std::vector<float> const &onsetsInSeconds){
	if ((!wave.size()) || (!onsetsInSeconds.size())){
		return std::nullopt;
	}
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, _analysisSettings, _splitSettings);
#pragma message("probably need some normalization, possibly based on variance")
	
	vecVecReal timbre_points;
	
	for (vecReal const &e : events){
		std::vector<float> event_measurements;
		
		EventwiseBFCCDescription bfcc_descr = calculateEventwiseBFCCDescription(e);
		event_measurements.insert(event_measurements.end(),
								  bfcc_descr.median.begin(), bfcc_descr.median.end());

		
		EventwisePitchDescription pitch_descr = calculateEventwisePitchDescription(e);
		auto const pitch_median = pitch_descr.median;
		event_measurements.push_back(pitch_median);
		
		timbre_points.push_back(event_measurements);

		std::cout << "got BFCCs for event\n";
	}
	std::cout << "calculated all BFCCs\n";
	return timbre_points;
}

std::optional<vecVecReal> Analyzer::calculatePCA(vecVecReal const &V){
	if (V.size() < 2){	// can't perform PCA with 1 sample
		return std::nullopt;
	}
	vecVecReal pca = nvs::analysis::PCA(V, ess_hold.standardFactory);
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
	
//	denormalizeOnsets(normalizedOnsets, getLengthInSeconds(wave.size(), analyzer._analysisSettings.sampleRate));
//	auto const &onsetsInSeconds = normalizedOnsets;	// merely renaming because now normalizedOnsets are in fact not normalized; they are in seconds.
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds,
														   analyzer.ess_hold.factory, analyzer._analysisSettings, analyzer._splitSettings);
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
											  analyzer._analysisSettings.sampleRate,//	double sampleRateToUse
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

