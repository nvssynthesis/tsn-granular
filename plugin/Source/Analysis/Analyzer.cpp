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
:	ess_init()  // don't delete this seemingly unnecessary construction-it is a good reminder that ess_init MUST be initialized first
,	ess_hold(ess_init)
{}

bool Analyzer::updateSettings(const juce::ValueTree &newSettings){
    // verify tree structure
    const bool valid = verifySettingsStructure(newSettings);
    jassert (newSettings.getParent().getChildWithName("FileInfo").hasProperty("sampleRate"));

    if (valid){
        updateSettingsFromValueTree(settings, newSettings);
        _settingsHash = util::hashValueTree(newSettings);
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
    return static_cast<float>(settings.analysis.sampleRate);
}

std::optional<vecReal> Analyzer::calculateOnsetsInSeconds(const vecReal &wave, RunLoopStatus& rls, const ShouldExitFn &shouldExit) const {
    if (wave.empty()){
        return std::nullopt;
    }

    const analysis::array2dReal onsets2d = calculateOnsetsMatrix(wave, ess_hold.factory, settings, rls, shouldExit);
    std::cout << "analyzed onsets\n";
    const essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();

#pragma message("it is a problem that we have not the ability to inject a runLoopCallback here, since onsetsInSeconds uses StandardFactory instead of StreamingFactory")

    std::vector<float> onsetsInSeconds = analysis::calculateOnsetsInSeconds(onsets2d, tmpStFac, settings);	// explicit namespace qualifier for clarity
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

    if (endIdx == -1) endIdx = static_cast<int>(frames.size());

    if (weights.size() != frames.size()) {
        throw EssentiaException("weights vector must match frames vector size");
    }

    uint vsize = frames[0].size();
    std::vector<T> result(vsize, static_cast<T>(0.0));
    T totalWeight = static_cast<T>(0.0);

    for (int i = beginIdx; i < endIdx; ++i) {
        T weight = weights[i];
        totalWeight += weight;

        for (uint j = 0; j < vsize; ++j) {
            result[j] += frames[i][j] * weight;
        }
    }

    // Normalize by total weight
    if (totalWeight > static_cast<T>(0.0)) {
        for (uint j = 0; j < vsize; ++j) {
            result[j] /= totalWeight;
        }
    }

    return result;
}


EventwisePitchDescription Analyzer::calculateEventwisePitchDescription(const vecReal &waveEvent) const {
    const auto [pitches, confidences] = calculatePitchesAndConfidences(waveEvent, settings);
#pragma message("not using confidences yet")

    // weightedMeanFrames()
    const auto p_mean = mean(pitches);
    const auto c_mean = mean(confidences);

    return EventwisePitchDescription {
        .pitch = {
            EventwiseStats {
                .mean = p_mean,
                .median = essentia::median(pitches),
                .variance = essentia::variance(pitches, p_mean),
                .skewness = essentia::skewness(pitches, p_mean),
                .kurtosis = essentia::kurtosis(pitches, p_mean)
            }
        },
        .confidence = {
            EventwiseStats {
                .mean = c_mean,
                .median = essentia::median(confidences),
                .variance = essentia::variance(confidences, c_mean),
                .skewness = essentia::skewness(confidences, c_mean),
                .kurtosis = essentia::kurtosis(confidences, c_mean)
            }
        }
    };
}

EventwiseLoudnessDescription Analyzer::calculateEventwiseLoudness(const vecReal &waveEvent) const {
    const vecReal l_tmp = calculateLoudnesses(waveEvent, settings);

    const auto l_mean = mean(l_tmp);

    const EventwiseLoudnessDescription descr {
            .mean = l_mean,
            .median = essentia::median(l_tmp),
            .variance = essentia::variance(l_tmp, l_mean),
            .skewness = essentia::skewness(l_tmp, l_mean),
            .kurtosis = essentia::kurtosis(l_tmp, l_mean)
    };
    return descr;
}

EventwiseBFCCDescription Analyzer::calculateEventwiseBFCCDescription(const vecReal &waveEvent) const {
    const vecVecReal b_tmp = calculateBFCCs(waveEvent, settings);	// bfccs per frame

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
    std::vector<EventwiseStats> descriptions;
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

auto Analyzer::calculateOnsetwiseTimbreSpace(const vecReal &wave,
                                        const std::vector<float> &onsetsInSeconds,
                                        RunLoopStatus& rls, const ShouldExitFn &shouldExit)
const -> std::optional<std::vector<FeatureContainer<EventwiseStats>>>
{
    if ((wave.empty()) || (onsetsInSeconds.empty())){
        return std::nullopt;
    }

    const double startMs = juce::Time::getMillisecondCounterHiRes();
    const auto   startTimeStr = juce::Time::getCurrentTime().toString (true, true, true, true);
    std::cout << "calculateOnsetwiseTimbreSpace start: " << startTimeStr << "\n";

    rls.set("Splitting Wave into Events...");

    const vecVecReal events = splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, settings, rls, shouldExit);
#pragma message("probably could benefit from some normalization, possibly based on variance")

    const size_t numEvents = events.size();
    std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbre_points(numEvents);

    const auto threadPoolOptions = ThreadPoolOptions()
        .withNumberOfThreads(settings.analysis.numThreads)
        .withDesiredThreadPriority(Thread::Priority::high)
        .withThreadName("TimbreAnalysis")
        .withThreadStackSizeBytes(Thread::osDefaultStackSize);
    ThreadPool pool(threadPoolOptions);

    std::atomic<size_t> completed {0};
    std::atomic<bool> cancelled {false};

    rls.set("Calculating timbre descriptions per event...");
    for (size_t i = 0; i < numEvents; ++i) {
        pool.addJob([&, i] {
            if (cancelled.load(std::memory_order_relaxed) || shouldExit()) {
                return;
            }
            const auto &e = events[i];
            FeatureContainer<EventwiseStats> f;

            f.bfccs = calculateEventwiseBFCCDescription(e);
            const auto [pitch, confidence] = calculateEventwisePitchDescription(e);
            f.f0 = pitch;
            f.periodicity = confidence;
            f.loudness = calculateEventwiseLoudness(e);

            timbre_points[i] = std::move(f);

            if (const auto numDone = ++completed;
                numDone % 4 == 0)
            {
                rls.set(static_cast<double>(numDone) / numEvents);
            }
            if (shouldExit()) {
                cancelled.store(true, std::memory_order_relaxed);
            }
        });
    }
    while (pool.getNumJobs() > 0) {
        Thread::sleep(10);  // sleep between checks
    }

    std::cout << "calculated all BFCCs\n";

    if (cancelled.load() || shouldExit()) {
        return std::nullopt;
    }

    const double endMs   = juce::Time::getMillisecondCounterHiRes();
    const auto   endTimeStr   = juce::Time::getCurrentTime().toString (true, true);
    const double elapsed = (endMs - startMs) * 0.001f;  // in seconds

    std::cout << "calculateOnsetwiseTimbreSpace end:   " << endTimeStr << "\n";
    std::cout << "\t\t\t Elapsed time:   " << juce::String (elapsed, 3) << " seconds\n";

    return timbre_points;
}

std::optional<vecVecReal> Analyzer::calculatePCA(const std::vector<FeatureContainer<EventwiseStats>> &allFeatures,
                                                 const std::vector<Features> &featuresToUse,
                                                 const Statistic statToUse) {
    if (allFeatures.size() < 2){	// can't perform PCA with 1 sample
        return std::nullopt;
    }
    // gather desired features into vecVecReal
    vecVecReal V;
    V.reserve(allFeatures.size());

    for (const auto &f : allFeatures){
        V.push_back(extractFeatures(f, featuresToUse, statToUse));
    }

    vecVecReal pca = PCA(V, 6);
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
    const vecVecReal events = splitWaveIntoEvents(wave, onsetsInSeconds,
                                            analyzer.ess_hold.factory,
                                            settings,
                                            rls,
                                            std::move(shouldExit));
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer;


    juce::File directory(base_name + analyzer.getSettingsHash());
    if (!directory.isDirectory()) {
        // If base_name has an extension, remove it to make it a directory
        directory = directory.getParentDirectory().getChildFile(directory.getFileNameWithoutExtension());
    }
    if (!directory.exists()) {
        if (const auto result = directory.createDirectory(); !result) {
            std::cerr << "Failed to create directory: " << result.getErrorMessage() << "\n";
            jassertfalse;
            return;
        }
    }
    std::cout << "Directory created at: " << directory.getFullPathName() << "\n";
    const juce::String filePrefix = juce::File(base_name).getFileName();

    int idx = 0;
    for (const auto &e : events){

        juce::String evName = filePrefix;
        evName << "_" << idx++ << ".wav";

        const juce::File outFile = directory.getChildFile(evName);

        juce::AudioBuffer<float> buffer(1, static_cast<int>(e.size()));
        buffer.clear();
        buffer.addFrom(0, 0, e.data(), static_cast<int>(e.size()));
        //			buffer.applyGainRamp(0, 0, 									5, 0.f, 1.f);	// fade in 5 samps
        //			buffer.applyGainRamp(0, static_cast<int>(e.size())-10, 10, 1.f, 0.f);	// fade out 10 samps


        const auto options = juce::AudioFormatWriterOptions()
                        .withSampleRate(analyzer.getAnalyzedFileSampleRate()) // sr
                        .withNumChannels(buffer.getNumChannels()) // numChans
                        .withBitsPerSample(24) // bitsPerSample
                        .withMetadataValues({}) // metadataValues
                        .withQualityOptionIndex(0) // qualityOptionIndex
                        .withSampleFormat(AudioFormatWriterOptions::SampleFormat::integral); // sampleFormat

        std::unique_ptr<OutputStream> outputStream = std::make_unique<FileOutputStream>(outFile);
        writer = format.createWriterFor(outputStream, options);

        if (writer != nullptr){
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            std::cout << "File " << outFile.getFileName() << " written\n";
        } else {
            std::cerr << "File write " << outFile.getFileName() << " fail\n";
        }
    }
}

}
