/*
  ==============================================================================

    Analyzer.h
    Created: 12 Sep 2023 10:25:24pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <optional>
#include "onsetAnalysis/OnsetAnalysis.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

class Analyzer {
private:
	nvs::ess::EssentiaInitializer ess_init;
public:
	Analyzer()	:	ess_hold(ess_init)
	,	_onsetSettings(_analysisSettings)
	,	_splitSettings(_analysisSettings)
	{}
	
	std::optional<std::vector<float>> calculateOnsets(std::vector<float> wave){
		if (!wave.size()){
			return std::nullopt;
		}
		
		analysis::array2dReal onsets2d = onsetAnalysis(wave, ess_hold.factory, _analysisSettings);
		essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
		return analysis::onsetsInSeconds(onsets2d, tmpStFac, _onsetSettings);
	}
	
	nvs::ess::EssentiaHolder ess_hold;
	
	analysis::analysisSettings _analysisSettings;
	analysis::onsetSettings _onsetSettings;
	analysis::splitSettings _splitSettings;

};


inline void writeEventsToWav(std::vector<float> wave, std::vector<float> onsetsInSeconds, std::string_view ogPath, essentia::streaming::AlgorithmFactory const &factory, nvs::analysis::splitSettings settings)
{
	if ( !wave.size() | !onsetsInSeconds.size() ){
		std::cerr << "unsuccessful write; wave or onsets of size 0\n";
		return;
	}
	juce::String name(ogPath.data());
	name.dropLastCharacters(4);
	
	nvs::analysis::vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, factory, settings);
	juce::WavAudioFormat format;
	std::unique_ptr<juce::AudioFormatWriter> writer;

	int idx = 0;
	for (auto const &e : events){
		juce::String evName = name;
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
											  settings.as.sampleRate,		//	double sampleRateToUse
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

