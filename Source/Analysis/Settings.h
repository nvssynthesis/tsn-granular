/*
  ==============================================================================

    Settings.h
    Created: 30 Oct 2023 2:15:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "essentia/types.h"

namespace nvs {
namespace analysis {

using essentia::Real;

// settings structs
struct analysisSettings {
	Real sampleRate {44100.f};
	int frameSize {1024};
	int hopSize {512};
	
	enum Features_e {
		BFCC,
		pitch,
		pitchConfidence
	};
	std::set<Features_e> featuresSet {
		BFCC,
		pitch
	};
	enum DimensionalityReduction_e {
		noReduction = 0,
		PCA = 1
	};
	DimensionalityReduction_e dimensionalityReduction { noReduction };
	
	const static inline std::map<Features_e, std::string>
	featuresMap {
		{BFCC, "BFCC"},
		{pitch, "Pitch"},
		{pitchConfidence, "Pitch Confidence"}
	};
	const static inline std::map<DimensionalityReduction_e, std::string>
	dimensionalityReductionMap {
		{noReduction, "No Reduction"},
		{PCA, "PCA"}
	};
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
	normalize_e normType {normalize_e::unit_max};
	enum spectrumType_e {
		magnitude = 0,
		power
	};
	spectrumType_e specType {spectrumType_e::power};
	enum weightingType_e {
		warping = 0,
		linear
	};
	weightingType_e weightType {weightingType_e::linear};
	enum dctType_e {
		typeII = 2,
		typeIII = 3
	};
	int dctType {dctType_e::typeII};

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
	
	std::string getNormalizeTypeAsString() const {return normMap.at(normType);}
	std::string getSpectrumTypeAsString() const {return spectrumTypeMap.at(specType);}
	std::string getWeightingTypeAsString() const {return weightingTypeMap.at(weightType);}
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
struct pitchSettings
{
	enum pitchDetectionAlgorithm_e {
		yin = 0,
		pYin,
		chroma
	};
	pitchDetectionAlgorithm_e algo {pitchDetectionAlgorithm_e::yin};
	bool interpolate {true};
	Real maxFrequency {22050.f};
	Real minFrequency {20.f};
	Real tolerance {0.15f};
	
	const static inline std::map<pitchDetectionAlgorithm_e, std::string>
	pitchDetectionAlgorithmMap {
		{yin, "PitchYin"},
		{pYin, "PitchYinProbabilistic"},
		{chroma, "Chromagram"}
	};
	std::string getPitchDetectionAlgoAsString() const { return pitchDetectionAlgorithmMap.at(algo); }
};
struct splitSettings
{
	size_t fadeInSamps {0};
	size_t fadeOutSamps {0};
};

}	// namespace analysis
}	// namespace nvs
