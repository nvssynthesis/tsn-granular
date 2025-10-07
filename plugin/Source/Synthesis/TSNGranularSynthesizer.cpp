/*
  ==============================================================================

    JuceTsaraGranularSynthesizer.cpp
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TSNGranularSynthesizer.h"
#include "../Synthesis/TSNPolyGrain.h"
#include "../../slicer_granular/Source/Synthesis/GranularSound.h"


// TODO:
// make generic function that can apply incoming arguments to generic function of tsnGuts of all voices, regardless of number of args

// definition for TSN specialization needed here because GranularVoice.h does not need to know about TSNPolyGrain
namespace nvs::gran {
template<>
void GranularVoice::initSynthGuts<nvs::gran::TSNPolyGrain>() {
    granularSynthGuts = std::make_unique<nvs::gran::TSNPolyGrain>(_synth_shared_state, &_voice_shared_state);
}

TSNGranularSynthesizer::TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts) :
    GranularSynthesizer(apvts)  // this is bad: we are expensively constructing the stripped down granular synth voices and then redundantly creating the actually needed TSN synth voices
,   _navigator(apvts)
{
    clearVoices();
    {
        _synth_shared_state._settings._center_position_at_env_peak = false;
        _synth_shared_state._settings._duration_pitch_compensation = 0.f;
    }
    unsigned long seed = 1234567890UL;
    for (int i = 0; i < num_voices; ++i) {
        const auto voice = GranularVoice::create<nvs::gran::TSNPolyGrain>(&_synth_shared_state, seed, i);
        addVoice(voice);
        ++seed;
    }
    clearSounds();
    addSound(new GranularSound);

    apvts.addParameterListener("navigator_type", this); // for setting navigation strategy
    apvts.state.addListener(&_timbreSpace);
    _timbreSpace.addActionListener(this);

    _navigator.setNavigationPeriod(50);
}
TSNGranularSynthesizer::~TSNGranularSynthesizer() {
    _synth_shared_state._apvts.state.removeListener(&_timbreSpace);
    _timbreSpace.removeActionListener(this);
}
//==============================================================================
void TSNGranularSynthesizer::parameterChanged(const String &parameterID, float newValue) {
    if (parameterID == "navigator_type") {
        const auto strategy = static_cast<nvs::timbrespace::NavigationType_e>(newValue);
        _navigator.setNavigationStrategy(strategy);
    }
}
void TSNGranularSynthesizer::actionListenerCallback(juce::String const &message) {
    if (message.compare("reportAvailability") == 0){
        auto onsetsOpt = _timbreSpace.getOnsets();
        if (onsetsOpt.has_value() && onsetsOpt->size()){
            loadOnsets(onsetsOpt.value());
        }
    }
}
void TSNGranularSynthesizer::loadOnsets(const std::span<float> onsets) {
    auto const numVoices = getNumVoices();
    for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
        if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){

            if (auto* tsnGuts = dynamic_cast<nvs::gran::TSNPolyGrain*>( granularVoice->getGranularSynthGuts() )){
                tsnGuts->loadOnsets(onsets);
            }
        }
    }
}

void TSNGranularSynthesizer::setReadBoundsFromChosenPoint() {
    // needs to get called upon each new navigation
    auto const &pIndices = _timbreSpace.getCurrentPointIndices();
    auto const onsetOpt = _timbreSpace.getOnsets();

    if (!onsetOpt.has_value() || (onsetOpt.value().size() == 0)){
        return;
    }

    /*
     this needs to happen AFTER proper onsets are loaded; otherwise the indices could be out of bounds
     However, since setWaveEvents happens based on a separate timer, the processing currently just exits early if the weighted indices exceed the numOnsets
    */
    constexpr auto numVoices = getNumVoices();
    for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
        if (const auto granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){

            if (nvs::gran::TSNPolyGrain* tsnGuts = dynamic_cast<nvs::gran::TSNPolyGrain*>( granularVoice->getGranularSynthGuts() )){
                tsnGuts->setWaveEvents(pIndices);
            }
        }
    }
}
void TSNGranularSynthesizer::setCurrentPlaybackSampleRate(const double newSampleRate) {
    _navigator.setSampleRate(newSampleRate);
    GranularSynthesizer::setCurrentPlaybackSampleRate(newSampleRate);
}
void TSNGranularSynthesizer::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    const auto p5D = _navigator.process(_timbreSpace, buffer.getNumSamples());

    jassert(p5D.norm() < 100.f);

    const auto &apvts = _synth_shared_state._apvts;

    const float K_neighbors = *apvts.getRawParameterValue("nav_selection_neighborhood");
    const float sharpness = *apvts.getRawParameterValue("nav_selection_sharpness");
    const float higher3Dweight = 0.05f;

    _timbreSpace.setTargetPoint(p5D);
    _timbreSpace.computeExistingPointsFromTarget(K_neighbors, sharpness, higher3Dweight, TimbreSpace::PointSelectionMethod::TRIANGULATION_BASED);

    setReadBoundsFromChosenPoint();

    // Synthesize
    renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
}
}   // namespace nvs::gran