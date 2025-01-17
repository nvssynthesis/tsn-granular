/*
  ==============================================================================

    Analyzer.cpp
    Created: 27 Sep 2023 12:25:12am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analysis/Analyzer.h"

namespace nvs {
namespace analysis {

Analyzer::Analyzer()
:	ess_hold(ess_init)
{}
	
std::optional<vecReal> Analyzer::calculateOnsets(vecReal wave, std::function<bool(void)> runLoopCallback){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = onsetAnalysis(wave, ess_hold.factory, _analysisSettings, runLoopCallback);
	std::cout << "analyzed onsets\n";
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")
	
	std::vector<float> onsetsInSeconds = analysis::onsetsInSeconds(onsets2d, tmpStFac, _analysisSettings, _onsetSettings);
	std::cout << "calculated onsets in seconds\n";

	float const sr = _analysisSettings.sampleRate;
	assert(sr > 0.f);
	float const lengthInSeconds = wave.size() / sr;
	cleanOnsets(onsetsInSeconds, lengthInSeconds);
	return onsetsInSeconds;
}

std::optional<vecVecReal> Analyzer::calculateOnsetwiseBFCCs(vecReal wave, std::vector<float> onsetsInSeconds){
	if ((!wave.size()) || (!onsetsInSeconds.size())){
		return std::nullopt;
	}
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, _analysisSettings, _splitSettings);
#pragma message("probably need some normalization, possibly based on variance")
	
	vecVecReal bfccs;
	for (vecReal const &e : events){
		std::span<float const > waveSpan(e);
		vecVecReal b_tmp = getBFCCs(waveSpan, ess_hold.factory, _analysisSettings, _bfccSettings);
		b_tmp = truncate(b_tmp, 5);
		vecReal binwiseMeans = binwiseMean(b_tmp);
		bfccs.push_back(binwiseMeans);
		std::cout << "got BFCCs for event\n";
	}
	std::cout << "calculated all BFCCs\n";
	return bfccs;
}

std::optional<vecVecReal> Analyzer::calculatePCA(vecVecReal const &V){
	if (V.size() < 2){	// can't perform PCA with 1 sample
		return std::nullopt;
	}
	vecVecReal pca = nvs::analysis::PCA(V, ess_hold.standardFactory);
	std::cout << "calculated PCAs\n";
	return pca;
}

Real mean(vecReal const &V){
	Real mean {0.f};
	for (auto const &e : V){
		mean += e;
	}
	mean /= static_cast<float>(V.size());
	return mean;
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
vecReal binwiseMean(vecVecReal const &V){
	vecVecReal Vtranspose = transpose(V);
	// now need to create a vector constituted of the mean of each vector in Vtranspose
	size_t const sz = Vtranspose.size();
	vecReal means(sz);
	for (int i = 0; i < sz; ++i){
		means[i] = mean(Vtranspose[i]);
	}
	return means;
}

void writeEventsToWav(vecReal wave, std::vector<float> onsetsInSeconds, std::string_view ogPath, Analyzer analyzer)
{
	if ( !wave.size() | !onsetsInSeconds.size() ){
		std::cerr << "unsuccessful write; wave or onsets of size 0\n";
		return;
	}
	juce::String const base_name(ogPath.data());
	base_name.dropLastCharacters(4);
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, analyzer.ess_hold.factory, analyzer._analysisSettings, analyzer._splitSettings);
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

