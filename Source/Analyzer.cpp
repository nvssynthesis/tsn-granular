/*
  ==============================================================================

    Analyzer.cpp
    Created: 27 Sep 2023 12:25:12am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analyzer.h"

namespace nvs {
namespace analysis {

Analyzer::Analyzer()
:	ess_hold(ess_init)
{}
	
std::optional<std::vector<float>> Analyzer::calculateOnsets(std::vector<float> wave){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = onsetAnalysis(wave, ess_hold.factory, _analysisSettings);
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
	std::vector<float> onsetsInSeconds = analysis::onsetsInSeconds(onsets2d, tmpStFac, _analysisSettings, _onsetSettings);
	
	float const sr = _analysisSettings.sampleRate;
	assert(sr > 0.f);
	float const lengthInSeconds = wave.size() / sr;
	cleanOnsets(onsetsInSeconds, lengthInSeconds);
	return onsetsInSeconds;
}

std::optional<std::vector<std::vector<float>>> Analyzer::calculateOnsetwiseBFCCs(std::vector<float> wave, std::vector<float> onsetsInSeconds){
	if ((!wave.size()) || (!onsetsInSeconds.size())){
		return std::nullopt;
	}
	
	vecVecReal events = nvs::analysis::splitWaveIntoEvents(wave, onsetsInSeconds, ess_hold.factory, _analysisSettings, _splitSettings);
#pragma message("probably need some normalization, possibly based on variance")
	
	std::vector<std::vector<float>> bfccs;
	for (vecReal const &e : events){
		std::span<float const > waveSpan(e);
		vecVecReal b_tmp = getBFCCs(waveSpan, ess_hold.factory, _analysisSettings, _bfccSettings);
		b_tmp = truncate(b_tmp, 5);
		vecReal binwiseMeans = binwiseMean(b_tmp);
		bfccs.push_back(binwiseMeans);
	}
	return bfccs;
}

std::optional<vecVecReal> Analyzer::PCA(vecVecReal const &V){
	if (!V.size()){
		return std::nullopt;
	}
	vecVecReal pca = nvs::analysis::PCA(V, ess_hold.standardFactory);
	return pca;
}

Real mean(vecReal const &V){
	Real mean {0.f};
	for (auto const &e : V){
		mean += e;
	}
	mean /= static_cast<float>(V.size());
	return mean;
}
vecVecReal truncate(vecVecReal const &V, size_t trunc){
	if (V.size() < trunc){
		return V;
	}
	vecVecReal Vtrunc(trunc);
	for (int i = 0; i < trunc; ++i){
		Vtrunc[i] = V[i];
	}
	return Vtrunc;
};

vecVecReal transpose(vecVecReal const &V){
	size_t const D0 = V.size();
	size_t const D1 = V[0].size();
	for (auto const &v : V){
		assert (v.size() == D1);
	}
	
	vecVecReal Vtranspose(D1);
	for (int i = 0; i < D1; ++i){
		Vtranspose[i].resize(D0);
		for (int j = 0; j < D0; ++j){
			Vtranspose[i][j] = V[j][i];
		}
	}
	return Vtranspose;
}
vecReal binwiseMean(vecVecReal const &V){
	vecVecReal Vtranspose = transpose(V);
	// now need to create a vector constituted of the mean of each vector in Vtranspose
	size_t const sz = Vtranspose.size();
	vecReal means(sz);
	for (int i = 0; i < sz; ++i){
		means[i] = mean(Vtranspose[i]);
	}
	return means;
}


}	// namespace analysis
}	// namespace nvs

