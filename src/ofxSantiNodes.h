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
#include "boolToVoid.h"
#include "voiceStealing.h"
#include "ocurrence.h"
#include "table.h"
#include "tableRowId.h"
#include "vectorRegion.h"
#include "MPGeneTable.h"
#include "verticalProfileTable.h"
#include "binPermute.h"
#include "mergeVoid.h"
#include "probSeq.h"
#include "soloSequencer.h"
#include "texUniForms.h"
#include "equalLoudness.h"
#include "multiStateVector.h"
#include "multiSliderMatrix.h"
#include "polySeq.h"
#include "choose.h"
#include "csv2vector.h"
#include "vectorStorage.h"
#include "vectorSetter.h"
#include "noteMatrix.h"
#include "indexMonitor.h"
#include "phasorSwing.h"
//#include "textureColumnCurve.h"
#include "valueIndex.h"
#include "valuesChanged.h"
#include "shell.h"
#include "buttonMatrix.h"
#include "vectorSequencer.h"
#include "soloStepSequencer.h"
#include "pianoRoll.h"
#include "voiceExpanding.h"
#include "voidToTick.h"
#include "TimeBetweenEvents.h"


//#include "spectrumLines.h"
//#include "pixelStretchVector.h"
//#include "vectorSymmetry.h"

namespace ofxOceanodeSanti{

static void registerModels(ofxOceanode &o)
{
    o.registerModel<beatMeasures>("Santi/AudioUtils");
    o.registerModel<BPMControl>("Santi/AudioUtils");
    o.registerModel<chancePass>("Santi/Chance");
    o.registerModel<chanceWeights>("Santi/Chance");
    o.registerModel<change>("Santi/General");
    o.registerModel<colorToVector>("Santi/Color");
    o.registerModel<vectorToColor>("Santi/Color");
    o.registerModel<conversions>("Santi/Conversions");
    o.registerModel<counterReset>("Santi/General");
    o.registerModel<countNumber>("Santi/General");
    o.registerModel<cycleCount>("Santi/General");
    o.registerModel<dataBuffer>("Santi/General");
    o.registerModel<distribute>("Santi/General");
    o.registerModel<euclideanPatterns>("Santi/Sequencers");
    o.registerModel<euclideanTicks>("Santi/Sequencers");
    o.registerModel<euclideanTicksPoly>("Santi/Sequencers");
    o.registerModel<eventCounter>("Santi/Events");
    o.registerModel<eventGate>("Santi/Events");
    o.registerModel<filterDuplicates>("Santi/Vectors");
    o.registerModel<frameGate>("Santi/Utils");
    o.registerModel<harmonicSeries>("Santi/AudioUtils");
    o.registerModel<indexHighlight>("Santi/Vectors");
    o.registerModel<limitPolyphony>("Santi/Voicing");
    o.registerModel<Logic>("Santi/Math");
    o.registerModel<merger>("Santi/General");
    o.registerModel<order>("Santi/Events");
    o.registerModel<permutations>("Santi/Vectors");
    o.registerModel<pixelStretch>("Santi/Textures");
    o.registerModel<polyFill>("Santi/?");
    o.registerModel<presetLoadTrigger>("Santi/General");
    o.registerModel<quantize>("Santi/Math");
    o.registerModel<rampTrigger>("Santi/General");
    o.registerModel<randomSeries>("Santi/Chance");
    o.registerModel<randomWalk>("Santi/Chance");
    o.registerModel<resetPhasor>("Santi/General");
    o.registerModel<scramble>("Santi/Chance");
    o.registerModel<segmentLength>("Santi/Utils");
    o.registerModel<slope>("Santi/Math");
    o.registerModel<splitMinMax>("Santi/General");
    o.registerModel<splitRoute>("Santi/General");
    o.registerModel<textureFlip>("Santi/Textures");
    o.registerModel<trigger>("Santi/Events");
    o.registerModel<UnrepeatedRandom>("Santi/Chance");
    o.registerModel<vecFilter>("Santi/Vectors");
    o.registerModel<vectorBlur>("Santi/Vectors");
    o.registerModel<vectorFeedback>("Santi/Vectors");
    o.registerModel<vectorFire>("Santi/Events");
    o.registerModel<vectorPointer>("Santi/Vectors");
    o.registerModel<split>("Santi/Vectors");
    o.registerModel<vectorTimer>("Santi/General");
    o.registerModel<voidToGate>("Santi/Conversions");
    o.registerModel<boolToVoid>("Santi/Conversions");
    o.registerModel<floatToBool>("Santi/Conversions");
    o.registerModel<boolToFloat>("Santi/Conversions");
    o.registerModel<floatToVoid>("Santi/Conversions");
    o.registerModel<voiceStealing>("Santi/Voicing");
    o.registerModel<Ocurrence>("Santi/General");
    o.registerModel<table>("Santi/Tables");
    o.registerModel<binPermute>("Santi/Utils");
    o.registerModel<mergeVoid>("Santi/General");
    o.registerModel<probSeq>("Santi/Sequencers");
    o.registerModel<soloSequencer>("Santi/Sequencers");
   o.registerModel<texUniForms>("Santi/Forms");
    o.registerModel<EqualLoudness>("Santi/AudioUtils");
    o.registerModel<multistateVector>("Santi/General");
    o.registerModel<multiSliderMatrix>("Santi/General");
    o.registerModel<polySeq>("Santi/Sequencers");
    o.registerModel<choose>("Santi/Chance");
    o.registerModel<csv2vector>("Santi/Conversions");
    o.registerModel<vectorStorage>("Santi/Vectors");
    o.registerModel<vectorSetter>("Santi/Vectors");
    o.registerModel<noteMatrix>("Santi/Sequencers");
    o.registerModel<indexMonitor>("Santi/Utils");
    o.registerModel<phasorSwing>("Santi/AudioUtils");
    //o.registerModel<textureColumnCurve>("Santi");
    o.registerModel<valueIndex>("Santi/Vectors");
    o.registerModel<valuesChanged>("Santi/Vectors");
    o.registerModel<shell>("Santi/Utils");
    o.registerModel<buttonMatrix>("Santi/Sequencers");
    o.registerModel<vectorSequencer>("Santi/Sequencers");
    o.registerModel<soloStepSequencer>("Santi/Sequencers");
    o.registerModel<pianoRoll>("Santi/Sequencers");
    o.registerModel<voiceExpanding>("Santi/Voicing");
    o.registerModel<voidToTick>("Santi/Events");
    o.registerModel<TimeBetweenEvents>("Santi/Events");
    //o.registerModel<spectrumLines>("Santi");
    //o.registerModel<pixelStretchVector>("Santi");
    //o.registerModel<vectorSymmetry>("Santi");
    o.registerModel<tableRowId>("Santi/Thalastasi");
    o.registerModel<vectorRegion>("Santi/Thalastasi");
    o.registerModel<geneTable>("Santi/Thalastasi");
    o.registerModel<verticalProfile>("Santi/Thalastasi");

}
}
#endif /* ofxSantiNodes_h */
