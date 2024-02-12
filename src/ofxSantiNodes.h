#ifndef ofxSantiNodes_h
#define ofxSantiNodes_h

// to use : ofApp.cpp include + register :
// #include "ofxSantiNodes.h"
// ofxOceanodeSanti::registerModels(oceanode);

#include "beatMeasures.h"
#include "BPMControl.h"
#include "chancePass.h"
#include "chanceWeights.h"
#include "change.h"
#include "colorToVector.h"
#include "conversions.h"
#include "counterReset.h"
#include "countNumber.h"
#include "cycleCount.h"
#include "dataBuffer.h"
#include "distribute.h"
#include "euclideanPatterns.h"
#include "euclideanTicks.h"
#include "euclideanTicksPoly.h"
#include "eventCounter.h"
#include "eventGate.h"
#include "filterDuplicates.h"
#include "frameGate.h"
#include "harmonicSeries.h"
#include "indexHighlight.h"
#include "limitPolyphony.h"
#include "Logic.h"
#include "merge.h"
#include "ofxSantiNodes.h"
#include "order.h"
#include "permutations.h"
#include "pixelStretch.h"
#include "polyFill.h"
#include "presetLoadTrigger.h"
#include "quantize.h"
#include "ramp.h"
#include "randomSeries.h"
#include "randomWalk.h"
#include "resetPhasor.h"
#include "scramble.h"
#include "segmentLength.h"
#include "slope.h"
#include "splitMinMax.h"
#include "splitRoute.h"
#include "textureFlip.h"
#include "trigger.h"
#include "unrepeatedRandom.h"
#include "vecFilter.h"
#include "vectorBlur.h"
#include "vectorFeedback.h"
#include "vectorFire.h"
#include "vectorPointer.h"
#include "vectorSplit.h"
#include "vectorTimer.h"
#include "voidToGate.h"

namespace ofxOceanodeSanti{

static void registerModels(ofxOceanode &o)
{
    o.registerModel<beatMeasures>("Santi");
    o.registerModel<BPMControl>("Santi");
    o.registerModel<chancePass>("Santi");
    o.registerModel<chanceWeights>("Santi");
    o.registerModel<change>("Santi");
    o.registerModel<colorToVector>("Santi");
    o.registerModel<conversions>("Santi");
    o.registerModel<counterReset>("Santi");
    o.registerModel<countNumber>("Santi");
    o.registerModel<cycleCount>("Santi");
    o.registerModel<dataBuffer>("Santi");
    o.registerModel<distribute>("Santi");
    o.registerModel<euclideanPatterns>("Santi");
    o.registerModel<euclideanTicks>("Santi");
    o.registerModel<euclideanTicksPoly>("Santi");
    o.registerModel<eventCounter>("Santi");
    o.registerModel<eventGate>("Santi");
    o.registerModel<filterDuplicates>("Santi");
    o.registerModel<frameGate>("Santi");
    o.registerModel<harmonicSeries>("Santi");
    o.registerModel<indexHighlight>("Santi");
    o.registerModel<limitPolyphony>("Santi");
    o.registerModel<Logic>("Santi");
    o.registerModel<merger>("Santi");
    o.registerModel<order>("Santi");
    o.registerModel<permutations>("Santi");
    o.registerModel<pixelStretch>("Santi");
    o.registerModel<polyFill>("Santi");
    o.registerModel<presetLoadTrigger>("Santi");
    o.registerModel<quantize>("Santi");
    o.registerModel<rampTrigger>("Santi");
    o.registerModel<randomSeries>("Santi");
    o.registerModel<randomWalk>("Santi");
    o.registerModel<resetPhasor>("Santi");
    o.registerModel<scramble>("Santi");
    o.registerModel<segmentLength>("Santi");
    o.registerModel<slope>("Santi");
    o.registerModel<splitMinMax>("Santi");
    o.registerModel<splitRoute>("Santi");
    o.registerModel<textureFlip>("Santi");
    o.registerModel<trigger>("Santi");
    o.registerModel<UnrepeatedRandom>("Santi");
    o.registerModel<vecFilter>("Santi");
    o.registerModel<vectorBlur>("Santi");
    o.registerModel<vectorFeedback>("Santi");
    o.registerModel<vectorFire>("Santi");
    o.registerModel<vectorPointer>("Santi");
    o.registerModel<split>("Santi");
    o.registerModel<vectorTimer>("Santi");
    o.registerModel<voidToGate>("Santi");
}
}
#endif /* ofxSantiNodes_h */