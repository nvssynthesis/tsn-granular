/*
  ==============================================================================

    TsaraGranularSynth.cpp
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TsaraGranularSynth.h"
namespace nvs	{
namespace gran	{

	TsaraGranular::TsaraGranular(float const &sampleRate, std::span<float> const &wavespan, size_t nGrains)
	:	genGranPoly1(sampleRate, wavespan, nGrains)
	,	ess_hold(ess_init)
	,	_onsetSettings(_analysisSettings)
	,	_splitSettings(_analysisSettings)
	{}
	void TsaraGranular::getOnsets(){
		std::vector<float> wave(_wavespan.size());
		std::copy(_wavespan.begin(), _wavespan.end(), wave.begin());
		analysis::array2dReal onsets2d = onsetAnalysis(wave, ess_hold.factory, _analysisSettings);
		essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
		onsetsSeconds = analysis::onsetsInSeconds(onsets2d, tmpStFac, _onsetSettings);
	}
	void TsaraGranular::writeEventsToWav(std::string_view ogPath){
		juce::String name(ogPath.data());
		name.dropLastCharacters(4);
		
		std::vector<float> wave(_wavespan.size());
		wave.assign(_wavespan.begin(), _wavespan.end());
		
		analysis::vecVecReal events = analysis::splitWaveIntoEvents(wave, onsetsSeconds, ess_hold.factory, _splitSettings);
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
												  _sampleRate,					//	double sampleRateToUse
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

}	// namespace gran
}	// namespace nvs
