//
//  ofxSantiNodesMath.cpp
//
//  Math, DSP and vectors: part of ofxOceanodeSanti::registerModels, in its own file
//  so the node headers compile in parallel and an edited node header rebuilds
//  only its part. registerModels runs the parts in their old order.
//

#include "ofxSantiNodes.h"
#include "ofxOceanode.h"
#include "ofxOceanodeTransport.h"
#include "circularCrossfade.h"
#include "derivative.h"
#include "Fold.h"
#include "formula.h"
#include "histogram2.h"
#include "histogramScore.h"
#include "Logic.h"
#include "metaballAnalyzer.h"
#include "polygonArea.h"
#include "polygonPerimeter.h"
#include "quantize.h"
#include "sigmoidCurve.h"
#include "slope.h"
#include "trigonometry.h"
#include "vectorBlur.h"
#include "vectorDeglitch.h"
#include "vectorFold.h"
#include "vectorInterpolation.h"
#include "vectorSplitOnMinusOne.h"
#include "binPermute.h"
#include "circularValueEaser.h"
#include "dataBuffer.h"
#include "dataBufferFeedbackMs.h"
#include "distribute.h"
#include "duplicator.h"
#include "filterDuplicates.h"
#include "ignoreZeros.h"
#include "indexHighlight.h"
#include "indexRouter.h"
#include "merge.h"
#include "mergeVoid.h"
#include "order.h"
#include "permutations.h"
#include "radialIndexer.h"
#include "segmentLength.h"
#include "splitMinMax.h"
#include "splitResize.h"
#include "splitRoute.h"
#include "valueIndex.h"
#include "valuesChanged.h"
#include "vecFilter.h"
#include "vectorExtract.h"
#include "vectorFeedback.h"
#include "vectorFile.h"
#include "vectorInsert.h"
#include "vectorInverter.h"
#include "vectorMorphology.h"
#include "vectorMorphologyVV.h"
#include "vectorOfVectorDisplay.h"
#include "vectorOfVectorIndexedSampler.h"
#include "vectorPointer.h"
#include "vectorRegion.h"
#include "vectorRegionVV.h"
#include "vectorSampler.h"
#include "vectorSetter.h"
#include "vectorSplit.h"
#include "vectorStorage.h"
#include "vectorSymmetry.h"
#include "vectorTimer.h"
#include "vectorToCoordinates.h"

namespace ofxOceanodeSanti{

void registerMathNodes(ofxOceanode &o)
{
    // ─────────────────────────────────────────────
    // MATH & DSP
    // ─────────────────────────────────────────────
    o.registerModel<circularCrossfade>("Santi/Math");
    o.registerModel<derivative>("Santi/Math");
    o.registerModel<fold>("Santi/Math");
    o.registerModel<formula>("Santi/Math");
    o.registerModel<histogram2>("Santi/Math");
    o.registerModel<histogramScore>("Santi/Math");
    o.registerModel<Logic>("Santi/Math");
    o.registerModel<metaballAnalyzer>("Santi/Math");
    o.registerModel<polygonArea>("Santi/Math");
    o.registerModel<polygonPerimeter>("Santi/Math");
    o.registerModel<quantize>("Santi/Math");
    o.registerModel<sigmoidCurve>("Santi/Math");
    o.registerModel<slope>("Santi/Math");
    o.registerModel<trigonometry>("Santi/Math");
    o.registerModel<vectorBlur>("Santi/Math");
    o.registerModel<vectorDeglitch>("Santi/Math");
    o.registerModel<vectorFold>("Santi/Math");
    o.registerModel<vectorInterpolation>("Santi/Math");
    o.registerModel<vectorSplitOnMinusOne>("Santi/Math");

    // ─────────────────────────────────────────────
    // VECTORS
    // ─────────────────────────────────────────────
    o.registerModel<binPermute>("Santi/Vectors");
    o.registerModel<circularValueEaser>("Santi/Vectors");
    o.registerModel<dataBuffer>("Santi/Vectors");
    o.registerModel<dataBufferFeedbackMs>("Santi/Vectors");
    o.registerModel<distribute>("Santi/Vectors");
    o.registerModel<duplicator>("Santi/Vectors");
    o.registerModel<filterDuplicates>("Santi/Vectors");
    o.registerModel<ignoreZeros>("Santi/Vectors");
    o.registerModel<indexHighlight>("Santi/Vectors");
    o.registerModel<indexRouter>("Santi/Vectors");
    o.registerModel<merger>("Santi/Vectors");
    o.registerModel<mergeVoid>("Santi/Vectors");
    o.registerModel<order>("Santi/Vectors");
    o.registerModel<permutations>("Santi/Vectors");
    o.registerModel<radialIndexer>("Santi/Vectors");
    o.registerModel<segmentLength>("Santi/Vectors");
    o.registerModel<splitMinMax>("Santi/Vectors");
    o.registerModel<splitResize>("Santi/Vectors");
    o.registerModel<splitRoute>("Santi/Vectors");
    o.registerModel<valueIndex>("Santi/Vectors");
    o.registerModel<valuesChanged>("Santi/Vectors");
    o.registerModel<vecFilter>("Santi/Vectors");
    o.registerModel<vectorExtract>("Santi/Vectors");
    o.registerModel<vectorFeedback>("Santi/Vectors");
    o.registerModel<vectorFile>("Santi/Vectors", ofxOceanodeNodeRegistrationFlags::Sink);
    o.registerModel<vectorInsert>("Santi/Vectors");
    o.registerModel<vectorInverter>("Santi/Vectors");
    o.registerModel<vectorMorphology>("Santi/Vectors");
    o.registerModel<vectorMorphologyVV>("Santi/Vectors");
    o.registerModel<vectorOfVectorDisplay>("Santi/Vectors");
    o.registerModel<vectorOfVectorIndexedSampler>("Santi/Vectors");
    o.registerModel<vectorPointer>("Santi/Vectors");
    o.registerModel<vectorRegion>("Santi/Vectors");
    o.registerModel<vectorRegionVV>("Santi/Vectors");
    o.registerModel<vectorSampler>("Santi/Vectors");
    o.registerModel<vectorSetter>("Santi/Vectors");
    o.registerModel<split>("Santi/Vectors");
    o.registerModel<vectorStorage>("Santi/Vectors");
    o.registerModel<vectorSymmetry>("Santi/Vectors");
    o.registerModel<vectorTimer>("Santi/Vectors");
    o.registerModel<vectorToCoordinates>("Santi/Vectors");
}

}
