//
// Created by Nicholas Solem on 9/26/25.
//

#ifndef RESOLOWPASS_H
#define RESOLOWPASS_H


namespace nvs::dsp {

class ResoLowpass
{
    // 2-pole lowpass used to smooth tendency point and other changes in an energy-preserving manner
    // for now adapted from https://www.musicdsp.org/en/latest/Filters/27-resonant-iir-lowpass-12db-oct.html
    //	resofreq = pole frequency
    //	amp = magnitude at pole frequency (approx)
public:
    void setSampleRate(double fs);

    void setParams(double frequency, double gain);

    double operator()(double v);

private:
    double _r, _c;
    double sampleRate;

    double vibrapos {0.0};
    double vibraspeed {0.0};
};

} // namespace nvs::dsp

#endif //RESOLOWPASS_H
