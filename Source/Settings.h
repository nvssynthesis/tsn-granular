/*
  ==============================================================================

    Settings.h
    Created: 30 Oct 2023 2:15:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <map>
#include <string>
#include "essentia/types.h"

namespace nvs {
namespace analysis {

using essentia::Real;

// settings structs
struct analysisSettings {
	Real sampleRate {44100.f};
	int frameSize {1024};
	int hopSize {512};
};
struct onsetSettings
{
	Real silenceThreshold {0.2f};
	Real alpha {0.1f};
	unsigned int numFrames_shortOnsetFilter {5};
	
	Real weight_hfc {1.f};
	Real weight_complex {1.f};
	Real weight_complexPhase {1.f};
	Real weight_flux {1.f};
	Real weight_melFlux {1.f};
	Real weight_rms {1.f};
	
	std::array<Real*, 6> weights {
		&weight_hfc,
		&weight_complex,
		&weight_complexPhase,
		&weight_flux,
		&weight_melFlux,
		&weight_rms
	};
};
struct bfccSettings
{
	Real highFrequencyBound {11000.f};
	Real lowFrequencyBound {0.f};
	//	int inputSize {1025};	get automatically from incoming spectrum
	
	int liftering {0};
	int numBands {40};
	int numCoefficients{13};
	
	enum normalize_e {
		unit_sum = 0,
		unit_max
	};
	enum spectrumType_e {
		magnitude = 0,
		power
	};
	enum weightingType_e {
		warping = 0,
		linear
	};
	enum dctType_e {
		typeII = 2,
		typeIII = 3
	};
	const static inline std::map<normalize_e, std::string>
	normMap {
		{unit_sum, "unit_sum"},
		{unit_max, "unit_max"}
	};
	const static inline std::map<spectrumType_e, std::string>
	spectrumTypeMap {
		{magnitude, "magnitude"},
		{power, "power"}
	};
	const static inline	std::map<weightingType_e, std::string>
	weightingTypeMap {
		{warping, "warping"},
		{linear, "linear"}
	};
	
	std::string normalizeType {normMap.at(normalize_e::unit_sum)};
	std::string spectrumType {spectrumTypeMap.at(spectrumType_e::power)};
	std::string weightingType {weightingTypeMap.at(weightingType_e::warping)};
	
	int dctType {dctType_e::typeII};
};
struct sBicSettings
{
	Real complexityPenaltyWeight {1.5f};
	int incrementFirstPass {60};	// min: 1
	int incrementSecondPass {20};
	int minSegmentLengthFrames {10};
	int sizeFirstPass {300};
	int sizeSecondPass {200};
};
struct splitSettings
{
	size_t fadeInSamps {0};
	size_t fadeOutSamps {0};
};

}	// namespace analysis
}	// namespace nvs
