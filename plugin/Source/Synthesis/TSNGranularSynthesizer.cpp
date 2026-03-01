/*
  ==============================================================================

    TSNGranularSynthesizer.cpp
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TSNGranularSynthesizer.h"
#include "../Synthesis/TSNPolyGrain.h"
#include "../../slicer_granular/Source/Synthesis/GranularSound.h"

// definition for TSN specialization needed here because GranularVoice.h does not need to know about TSNPolyGrain
namespace nvs::gran {
template<>
void GranularVoice::initSynthGuts<TSNPolyGrain>() {
    granularSynthGuts = std::make_unique<TSNPolyGrain>(_synth_shared_state, &_voice_shared_state);
}

TSNGranularSynthesizer::TSNGranularSynthesizer(AudioProcessorValueTreeState &apvts) :
    GranularSynthesizer(apvts)  // this is bad: we are expensively constructing the stripped down granular synth voices and then redundantly creating the actually needed TSN synth voices
,   _navigator(apvts)
,   _timbreSpace(apvts)
,   _timbreSpacePointSelector(apvts, _timbreSpace)
{
    clearVoices();
    {
        _synth_shared_state._settings._center_position_at_env_peak = false;
        _synth_shared_state._settings._duration_pitch_compensation = 0.f;
    }
    unsigned long seed = 1234567890UL;
    for (int i = 0; i < num_voices; ++i) {
        const auto voice = GranularVoice::create<TSNPolyGrain>(&_synth_shared_state, seed, i);
        addVoice(voice);
        if (auto* tsnGuts = dynamic_cast<TSNPolyGrain*>( voice->getGranularSynthGuts() ) ) {
            _tsn_polygrains[i] = tsnGuts;
        }
        ++seed;
    }
    clearSounds();
    addSound(new GranularSound);

    apvts.state.addListener(&_timbreSpace);
    _timbreSpace.addActionListener(this);

    _navigator.setNavigationPeriod(5.0);
}
TSNGranularSynthesizer::~TSNGranularSynthesizer() {
    _synth_shared_state._apvts.state.removeListener(&_timbreSpace);
    _timbreSpace.removeActionListener(this);
}
//==============================================================================
void TSNGranularSynthesizer::actionListenerCallback(const String &message) {
    if (message == axiom::tsn::onsetsAvailable) {
        // loadOnsets(_timbreSpace.shareOnsets());
    }
    if (message == axiom::tsn::timbreSpaceTreeChanged) {
        // update f0s
        for (auto *pg : _tsn_polygrains) {
            pg->setNeededData(_timbreSpace.shareOnsets(),_timbreSpace.getRawFeatureValues(analysis::Feature_e::f0, analysis::Statistic::Median));
        }
    }
}
//==============================================================================

void TSNGranularSynthesizer::setReadBoundsFromChosenPoint() {
    // needs to get called upon each new navigation
    if (const SharedOnsets onsetsResult = _timbreSpace.shareOnsets();
        onsetsResult == nullptr || onsetsResult->onsets.empty())
    {
        DBG("TSNGranularSynthesizer::setReadBoundsFromChosenPoint: onsets unavailable; returning\n");
        return;
    }

    /*
     this needs to happen AFTER proper onsets are loaded; otherwise the indices could be out of bounds
     However, since setWaveEvents happens based on a separate timer, the processing currently just exits
     early if the weighted indices exceed the numOnsets
    */
    auto const &pIndices = _timbreSpacePointSelector.getCurrentPointIndices();
    constexpr auto numVoices = getNumVoices();
    for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
        jassert(_tsn_polygrains[voiceIdx] != nullptr);
        _tsn_polygrains[voiceIdx]->setWaveEvents(pIndices);
    }
}
void TSNGranularSynthesizer::setCurrentPlaybackSampleRate(const double newSampleRate) {
    _navigator.setSampleRate(newSampleRate);
    GranularSynthesizer::setCurrentPlaybackSampleRate(newSampleRate);
}
void TSNGranularSynthesizer::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi)
{
    if (const auto navType = static_cast<timbrespace::NavigationType_e>(_synth_shared_state._apvts.getRawParameterValue("navigator_type")->load());
        _navigator.getNavigationStrategy() != navType)
    {
        _navigator.setNavigationStrategy(navType);
    }
    const auto p5D = _navigator.process(buffer.getNumSamples());

    jassert(p5D.norm() < 100.f);

    _timbreSpacePointSelector.computeExistingPointsFromTarget(p5D);

    setReadBoundsFromChosenPoint();

    // Synthesize
    renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
}
}   // namespace nvs::gran