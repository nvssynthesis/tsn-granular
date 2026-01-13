/*
  ==============================================================================

    TimbreAnalysis.cpp
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreAnalysis.h"

namespace nvs::analysis {

namespace {
PitchesAndConfidences calculatePitchesEssentiaYin(std::span<Real> waveSpan, AnalyzerSettings const& settings){
    vecReal wave(waveSpan.begin(), waveSpan.end());

    int const frameSize = settings.analysis.frameSize;
    int const zeroPadding = frameSize;

    auto frameCutter = std::unique_ptr<standard::Algorithm>(standardFactory::create ("FrameCutter",
                "frameSize",            frameSize,
                "hopSize",              settings.analysis.hopSize,
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    auto windowing = std::unique_ptr<standard::Algorithm>(standardFactory::create ("Windowing",
                "normalized", false,
                "size",        frameSize,
                "zeroPadding", zeroPadding,
                "type",        settings.analysis.windowingType.toStdString(),
                "zeroPhase",   false
            ));

    std::map<std::string, std::string> pitchAlgoNicknameMap {
            {"yin", "PitchYin"}
    };	// for now we only handle this

    auto pitchDet = std::unique_ptr<standard::Algorithm>(standardFactory::create (pitchAlgoNicknameMap[settings.pitch.pitchDetectionAlgorithm.toStdString()],
                "frameSize",   frameSize,
                "interpolate",  settings.pitch.interpolate,
                "maxFrequency", settings.pitch.maxFrequency,
                "minFrequency", settings.pitch.minFrequency,
                "sampleRate",   settings.analysis.sampleRate,
                "tolerance",    settings.pitch.tolerance
            ));

    vecReal frequencies, confidences; // accumulate results manually
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(wave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // detect pitch
        Real pitch, pitchConfidence;
        pitchDet->input("signal").set(windowedFrame);
        pitchDet->output("pitch").set(pitch);
        pitchDet->output("pitchConfidence").set(pitchConfidence);
        pitchDet->compute();

        // accumulate results
        frequencies.push_back(pitch);
        confidences.push_back(pitchConfidence);
    }

    assert(frequencies.size() == confidences.size());
    {
        // convert frequency to pitch
        vecReal pitches = std::move(frequencies);
        assert(pitches.size() == confidences.size());

        std::ranges::transform(pitches, pitches.begin(),
                               [](const float x) {
                                   if (x == 0.f) {
                                       return 0.0f;
                                   }
                                   return 69.f + 12.f * std::log2(x / 440.f);
                               }
                );
        return PitchesAndConfidences{pitches, confidences};
    }
}
}	// anonymous namespace

PitchesAndConfidences calculatePitchesAndConfidences (vecReal waveEvent,
                                                      AnalyzerSettings const& settings)
{
    auto const algo      = settings.pitch.pitchDetectionAlgorithm.toStdString();

    if (algo == "yin") {
        return calculatePitchesEssentiaYin (waveEvent, settings);
    }
    if (algo == "pYin") {
        jassertfalse;  // not implemented
        return {};
        //		return calculatePitchesEssentiaPYin (waveEvent, factory, settingsTree);
    }
    if (algo == "chroma") {
        jassertfalse;  // not implemented
        return {};
        //		return calculatePitchesEssentiaChroma (waveEvent, factory, settingsTree);
    }
    jassertfalse;  // unknown algorithm
    return {};
}

vecReal calculateLoudnesses(const std::span<Real const> waveSpan, AnalyzerSettings const& settings)
{
    auto const filteredWave = [&settings](std::span<Real const> _waveSpan) {
        const vecReal wave(_waveSpan.begin(), _waveSpan.end());
        if (!settings.loudness.equalizeLoudness) {
            return wave;
        }

        [[maybe_unused]] const float sampleRate = settings.analysis.sampleRate;

        const auto equalLoudnessFilter = std::unique_ptr<standard::Algorithm>(standardFactory::create(
                "EqualLoudness",
                "sampleRate", sampleRate
                )); // not using yet

        // apply equal loudness filter to entire wave first
        vecReal w;
        equalLoudnessFilter->input("signal").set(wave);
        equalLoudnessFilter->output("signal").set(w);
        equalLoudnessFilter->compute();
        return w;
    }(waveSpan);

    const int frameSize = settings.analysis.frameSize;
    const int hopSize = settings.analysis.hopSize;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(standardFactory::create (
            "FrameCutter",
            "frameSize",               frameSize,
            "hopSize",                 hopSize,
            "lastFrameToEndOfFile",    true,
            "startFromZero",           true,
            "validFrameThresholdRatio", 0.0
            ));

    const auto windowing = std::unique_ptr<standard::Algorithm>(standardFactory::create (
            "Windowing",
            "normalized",  false,
            "size",        frameSize,
            "zeroPadding", frameSize,
            "type",        settings.analysis.windowingType.toStdString(),
            "zeroPhase",   false
            ));

    const auto loudness = std::unique_ptr<standard::Algorithm>(standardFactory::create("Loudness"));

    vecReal loudnesses; // NOLINT

    // Process frame by frame
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(filteredWave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // calculate loudness
        Real loudnessValue;
        loudness->input("signal").set(windowedFrame);
        loudness->output("loudness").set(loudnessValue);
        loudness->compute();

        // accumulate result
        loudnesses.push_back(loudnessValue);
    }

    return loudnesses;
}

FeatureContainer<vecReal> calculateTimbres(std::span<Real const> waveSpan, AnalyzerSettings const& settings)
{
    vecReal wave(waveSpan.begin(), waveSpan.end());

    const int frameSize  = settings.analysis.frameSize;

    for (auto &e : wave) {
        const float normFactor = 1.f;   // settings.analysis.frameSize;
        e *= normFactor;
    }

    const int hopSize = settings.analysis.hopSize;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(standardFactory::create (
        "FrameCutter",
          "frameSize",               frameSize,
          "hopSize",                 hopSize,
          "lastFrameToEndOfFile",    true,
          "startFromZero",           true,
          "validFrameThresholdRatio", 0.0
    ));
    const auto windowing = std::unique_ptr<standard::Algorithm>(standardFactory::create (
        "Windowing",
          "normalized",  false,
          "size",        frameSize,
          "zeroPadding", frameSize, // why am i even zero padding?
          "type",        settings.analysis.windowingType.toStdString(),
          "zeroPhase",   false
    ));
    auto const spectrumTypeStr = settings.bfcc.spectrumType.toStdString();
    bool const isPower = (spectrumTypeStr == "power");
    std::string const specAlgoStr = isPower ? "PowerSpectrum" : "Spectrum";
    const auto spectrum = std::unique_ptr<standard::Algorithm>(standardFactory::create (
            specAlgoStr,
            "size", frameSize * 2
            ));
    std::map<juce::String, int> dctTypeStringToInt {
            { "typeII",  2 },
            { "typeIII", 3 }
    };
    std::map<std::string, std::string> logTypeMap {
            {"PowerSpectrum", "dbpow"},
            {"Spectrum", "dbamp"}
    };
    const auto sampleRate  = static_cast<float>(settings.analysis.sampleRate);
    const auto bfcc = std::unique_ptr<standard::Algorithm>(standardFactory::create (
    "BFCC",
    "dctType",             dctTypeStringToInt.at(settings.bfcc.dctType.toStdString()),
    "highFrequencyBound",  settings.bfcc.highFrequencyBound,
    "inputSize",           frameSize + 1,
    "liftering",           settings.bfcc.liftering,
    "logType",             logTypeMap.at(specAlgoStr),

    "lowFrequencyBound",   settings.bfcc.lowFrequencyBound,
    "normalize",           settings.bfcc.normalize.toStdString(),
    "numberBands",         settings.bfcc.numBands,
    "numberCoefficients",  settings.bfcc.numCoefficients,
    "sampleRate",          sampleRate,
    "type",                spectrumTypeStr,
    "weighting",           settings.bfcc.weightingType.toStdString()
    ));
    const auto centroid_a = std::unique_ptr<standard::Algorithm>(standardFactory::create ("Centroid",
        "range", sampleRate * 0.5));
    const auto decrease_a = std::unique_ptr<standard::Algorithm>(standardFactory::create ("Decrease",
        "range", sampleRate * 0.5));
    const auto flatnessDB_a = std::unique_ptr<standard::Algorithm>(standardFactory::create ("FlatnessDB"));
    const auto crest_a = std::unique_ptr<standard::Algorithm>(standardFactory::create ("Crest"));
    const auto spectralComplexity_a = std::unique_ptr<standard::Algorithm>(standardFactory::create ("SpectralComplexity",
        "magnitudeThreshold", settings.spectralComplexity.magnitudeThreshold));
    const auto strongPeakinesses_a = std::unique_ptr<standard::Algorithm>(standardFactory::create("StrongPeak"));

    std::string const specInputStr  = isPower ? "signal"        : "frame";
    std::string const specOutputStr = isPower ? "powerSpectrum" : "spectrum";

    // std::vector<std::vector<float>> barkBandsVV, BFCCsVV;

    FeatureContainer<vecReal> timbres;

    // Process frame by frame
    int frameCounter = 0;
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(wave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // compute spectrum
        vecReal spectrumVec;
        spectrum->input(specInputStr).set(windowedFrame);
        spectrum->output(specOutputStr).set(spectrumVec);
        spectrum->compute();

        // compute BFCC
        vecReal _, bfccVec;
        bfcc->input("spectrum").set(spectrumVec);
        bfcc->output("bands").set(_);
        bfcc->output("bfcc").set(bfccVec);
        bfcc->compute();
        pushBFCCFrame(timbres, bfccVec);

        Real centroid;
        centroid_a->input("array").set(spectrumVec);
        centroid_a->output("centroid").set(centroid);
        centroid_a->compute();
        timbres[Feature_e::SpectralCentroid].push_back(centroid);

        Real decrease;
        decrease_a->input("array").set(spectrumVec);
        decrease_a->output("decrease").set(decrease);
        decrease_a->compute();
        timbres[Feature_e::SpectralDecrease].push_back(decrease);

        Real flatness;
        flatnessDB_a->input("array").set(spectrumVec);
        flatnessDB_a->output("flatnessDB").set(flatness);
        flatnessDB_a->compute();
        timbres[Feature_e::SpectralFlatness].push_back(flatness);

        Real crest;
        crest_a->input("array").set(spectrumVec);
        crest_a->output("crest").set(crest);
        crest_a->compute();
        timbres[Feature_e::SpectralCrest].push_back(crest);

        Real spectralComplexity;
        spectralComplexity_a->input("spectrum").set(spectrumVec);
        spectralComplexity_a->output("spectralComplexity").set(spectralComplexity);
        timbres[Feature_e::SpectralComplexity].push_back(spectralComplexity);

        frameCounter++;
    }

    assert(!timbres.bfccs().empty());
    assert(!timbres.bfccs()[0].empty());
    const size_t expected_len = timbres.features[0].size();
    assert(std::ranges::all_of(
        timbres.features.begin(),
        timbres.features.begin() + NumTimbralFeatures,
        [expected_len](const auto &v)
    {
        return (v.size() == expected_len);
    }));

    return timbres;
}

vecVecReal PCA(vecVecReal const &V, int num_features_out){
    const std::string namespaceIn {"data"};
    const std::string namespaceOut {"pca"};

    standard::Algorithm* PCA = nvs::analysis::standardFactory::create("PCA",
                                                                      "dimensions", num_features_out,
                                                                      "namespaceIn", namespaceIn,
                                                                      "namespaceOut", namespaceOut);

    Pool inPool, outPool;
    for (auto v : V){
        inPool.add(namespaceIn, v);
    }

    PCA->input("poolIn").set(inPool);
    PCA->output("poolOut").set(outPool);
    PCA->compute();

    auto const &vecRealPool = outPool.getVectorRealPool();
    vecVecReal PCAmat = vecRealPool.at(namespaceOut);

    return PCAmat;
}

std::pair<Real, Real> calculateRangeOfDimension(vecVecReal const &V, const size_t dim){
    Real min {std::numeric_limits<Real>::max()};
    Real max {std::numeric_limits<Real>::lowest()};

    for (auto const &v : V){
        auto const val = v[dim];
        if (val < min){
            min = val;
        }
        if (val > max){
            max = val;
        }
    }

    return std::make_pair(min, max);
}

Real calculateNormalizationMultiplier(const std::pair<Real, Real> &range){
    return 1.f / std::max(std::abs(range.first), std::abs(range.second));
}

}
