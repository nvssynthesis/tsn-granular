//
// Created by Nicholas Solem on 9/26/25.
//

#include "ResoLowpass.h"
#include <JuceHeader.h>

namespace nvs::dsp {

void ResoLowpass::setSampleRate(double fs){
    sampleRate = fs;
}

void ResoLowpass::setParams(double fc, double gain){
    jassert(sampleRate > 0.0);

    double const w = 2.0 * juce::MathConstants<double>::pi * fc / sampleRate; // Pole angle
    double const q = 1.0 - w / (2.0 * (gain + 0.5 / (1.0 + w)) + w - 2.0); // Pole magnitude
    _r = q * q;
    _c = _r + 1.0 - 2.0 * cos(w) * q;
}

double ResoLowpass::operator()(double v){
    /* Accelerate vibra by signal-vibra, multiplied by lowpasscutoff */
    vibraspeed += (v - vibrapos) * _c;

    /* Add velocity to vibra's position */
    vibrapos += vibraspeed;

    /* Attenuate/amplify vibra's velocity by resonance */
    vibraspeed *= _r;

    /* Check clipping */
    double tmp = vibrapos;
    tmp = juce::jlimit(-32768.0, 32767.0, tmp);

    return tmp;
}

} // namespace nvs::dsp