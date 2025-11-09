/*
  ==============================================================================

    AnalysisUsing.h
    Created: 30 Oct 2023 2:35:10pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/EssentiaSetup.h"
#include "essentia/streaming/algorithms/poolstorage.h"
#include "essentia/scheduler/network.h"
#include "essentia/streaming/algorithms/vectorinput.h"
#include "essentia/streaming/algorithms/vectoroutput.h"
#include "essentia/utils/tnt/tnt2vector.h"

#include "essentia/essentiamath.h"

namespace nvs::analysis {

//===================================================================================
using namespace essentia;
using namespace essentia::streaming;
using namespace essentia::scheduler;
//===================================================================================
using vecReal = std::vector<Real>;
using vecVecReal = std::vector<vecReal>;
using vectorInput = essentia::streaming::VectorInput<Real> ;
using vectorInputCumulative = essentia::streaming::VectorInput<vecReal> ;
using vectorOutput = essentia::streaming::VectorOutput<Real>;
using vectorOutputCumulative = essentia::streaming::VectorOutput<vecReal> ;
//using matrixInput = essentia::streaming::VectorInput<std::vector<vecReal>> ;
using startAndEndTimesVec = std::pair<vecReal, vecReal> ;

using streamingFactory = essentia::streaming::AlgorithmFactory;
using standardFactory = essentia::standard::AlgorithmFactory;

using array2dReal = TNT::Array2D<Real>;

}   // namespace nvs::analysis
