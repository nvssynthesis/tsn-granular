/*
  ==============================================================================

    Analyzer.cpp
    Created: 27 Sep 2023 12:25:12am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analysis/Analyzer.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"
#include <concepts>

namespace nvs::analysis {

Analyzer::Analyzer()
:	ess_init()
,	ess_hold(ess_init)
{}

bool Analyzer::updateSettings(const juce::ValueTree &newSettings){
    // verify tree structure
    const bool valid = verifySettingsStructure(newSettings);
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

std::optional<vecReal> Analyzer::calculateOnsetsInSeconds(const vecReal &wave, RunLoopStatus& rls, const ShouldExitFn &shouldExit) const {
    if (!wave.size()){
        return std::nullopt;
    }

    analysis::array2dReal onsets2d = calculateOnsetsMatrix(wave, ess_hold.factory, settings, rls, shouldExit);
    std::cout << "analyzed onsets\n";
    const essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();

#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")

    std::vector<float> onsetsInSeconds = analysis::calculateOnsetsInSeconds(onsets2d, tmpStFac, settings);	// needs namespace qualifier to not call itself
    std::cout << "calculated onsets in seconds\n";

    return onsetsInSeconds;
}


template <typename T>
static std::vector<T> weightedMeanFrames(const std::vector<std::vector<T>>& frames,
                                   const std::vector<T>& weights,
                                   int beginIdx = 0,
                                   int endIdx = -1) {
    if (frames.empty()) {
        throw EssentiaException("trying to calculate mean of empty array of frames");
    }

    if (endIdx == -1) endIdx = (int)frames.size();

    if (weights.size() != frames.size()) {
        throw EssentiaException("weights vector must match frames vector size");
    }

    uint vsize = frames[0].size();
    std::vector<T> result(vsize, (T)0.0);
    T totalWeight = (T)0.0;

    for (int i = beginIdx; i < endIdx; ++i) {
        T weight = weights[i];
        totalWeight += weight;

        for (uint j = 0; j < vsize; ++j) {
            result[j] += frames[i][j] * weight;
        }
    }

    // Normalize by total weight
    if (totalWeight > (T)0.0) {
        for (uint j = 0; j < vsize; ++j) {
            result[j] /= totalWeight;
        }
    }

    return result;
}



EventwisePitchDescription Analyzer::calculateEventwisePitchDescription(const vecReal &waveEvent) const {
    const auto [pitches, confidences] = calculatePitchesAndConfidences(waveEvent, ess_hold.factory, settings);
#pragma message("not using confidences yet")
    const auto p_mean = mean(pitches);

    const EventwisePitchDescription descr {
            .mean = p_mean,
            .median = essentia::median(pitches),
            .variance = essentia::variance(pitches, p_mean),
            .skewness = essentia::skewness(pitches, p_mean),
            .kurtosis = essentia::kurtosis(pitches, p_mean)
    };
    return descr;
}

EventwiseStatistics<float> Analyzer::calculateEventwiseLoudness(const vecReal &waveEvent) const {
    const vecReal l_tmp = calculateLoudnesses(waveEvent, ess_hold.factory, settings);

    const auto l_mean = mean(l_tmp);

    const EventwisePitchDescription descr {
            .mean = l_mean,
            .median = essentia::median(l_tmp),
            .variance = essentia::variance(l_tmp, l_mean),
            .skewness = essentia::skewness(l_tmp, l_mean),
            .kurtosis = essentia::kurtosis(l_tmp, l_mean)
    };
    return descr;
}

EventwiseBFCCDescription Analyzer::calculateEventwiseBFCCDescription(const vecReal &waveEvent) const {
    const vecVecReal b_tmp = calculateBFCCs(waveEvent, ess_hold.factory, settings);	// bfccs per frame

    // const vecReal means = essentia::meanFrames(b_tmp);	// get mean per bfcc across all frames
    vecReal frameWeights; frameWeights.reserve(b_tmp.size());
    for (auto const &frame : b_tmp) {
        const Real bfcc0 = frame[0]; // bfcc #0 is energy
        const Real weight = std::exp(bfcc0 * settings._statistics.BFCC0_normalizationFactor);
        frameWeights.push_back(weight);
    }
    const vecReal means = weightedMeanFrames(b_tmp, frameWeights);
    const vecReal  medians = essentia::medianFrames(b_tmp);
    const vecReal  variances = essentia::varianceFrames(b_tmp);
    const vecReal  skewnesses = essentia::skewnessFrames(b_tmp);
    const vecReal  kurtoses = essentia::kurtosisFrames(b_tmp);

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
Analyzer::calculateOnsetwiseTimbreSpace(const vecReal &wave,
                                        const std::vector<float> &onsetsInSeconds,
                                        RunLoopStatus& rls, const ShouldExitFn &shouldExit) const {
    if ((wave.empty()) || (onsetsInSeconds.empty())){
        return std::nullopt;
    }

    const double startMs = juce::Time::getMillisecondCounterHiRes();
    const auto   startTimeStr = juce::Time::getCurrentTime().toString (true, true, true, true);
    std::cout << "calculateOnsetwiseTimbreSpace start: " << startTimeStr << "\n";

    rls.set("Splitting Wave into Events...");

    const vecVecReal events = splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, settings, rls, shouldExit);
#pragma message("probably need some normalization, possibly based on variance")

    std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbre_points;

    rls.set("Calculating timbre descriptions per event...");
    for (size_t i = 0; i < events.size(); ++i) {
        const auto &e = events[i];
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
    const auto   endTimeStr   = juce::Time::getCurrentTime().toString (true, true);
    const double elapsed = (endMs - startMs) * 0.001f;  // in seconds

    std::cout << "calculateOnsetwiseTimbreSpace end:   " << endTimeStr << "\n";
    std::cout << "\t\t\t Elapsed time:   " << juce::String (elapsed, 3) << " seconds\n";

    return timbre_points;
}

std::optional<vecVecReal> Analyzer::calculatePCA(const std::vector<FeatureContainer<EventwiseStats>> &allFeatures,
                                                 const std::vector<Features> &featuresToUse,
                                                 const Statistic statToUse) const
{
    if (allFeatures.size() < 2){	// can't perform PCA with 1 sample
        return std::nullopt;
    }
    // gather desired features into vecVecReal
    vecVecReal V;
    V.reserve(allFeatures.size());

    for (const auto &f : allFeatures){
        V.push_back(extractFeatures(f, featuresToUse, statToUse));
    }

    vecVecReal pca = PCA(V, ess_hold.standardFactory, 6);
    std::cout << "calculated PCAs\n";
    return pca;
}

vecVecReal truncate(const vecVecReal &V, const size_t trunc){
    if (V.size() < trunc){
        return V;
    }
    vecVecReal Vtrunc(trunc);
    for (size_t i = 0; i < trunc; ++i){
        Vtrunc[i] = V[i];
    }
    return Vtrunc;
};

vecVecReal transpose(const vecVecReal &V){
    size_t const D0 = V.size();
    size_t const D1 = V[0].size();
    for (const auto &v : V){
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

void writeEventsToWav(const vecReal &wave,
                      const std::vector<float> &onsetsInSeconds,
                      std::string_view ogPath,
                      const Analyzer &analyzer,
                      RunLoopStatus& rls,
                      ShouldExitFn shouldExit)
{
    if ( wave.empty() or onsetsInSeconds.empty() ){
        std::cerr << "unsuccessful write; wave or onsets of size 0\n";
        return;
    }
    const auto base_name = [ogPath]() -> juce::String {
        const juce::String s(ogPath.data());
        return s.dropLastCharacters(4);
    }();

    const auto& settings = analyzer.getSettings();
    //	denormalizeOnsets(normalizedOnsets, getLengthInSeconds(wave.size(), analyzer._analysisSettings.sampleRate));
    //	const auto &onsetsInSeconds = normalizedOnsets;	// merely renaming because now normalizedOnsets are in fact not normalized; they are in seconds.
    const vecVecReal events = splitWaveIntoEvents(wave, onsetsInSeconds,
                                            analyzer.ess_hold.factory,
                                            settings,
                                            rls,
                                            std::move(shouldExit));
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer;

    int idx = 0;
    for (const auto &e : events){
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

}
