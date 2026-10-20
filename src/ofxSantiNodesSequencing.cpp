//
//  ofxSantiNodesSequencing.cpp
//
//  Timing, sequencers, chance and events: part of ofxOceanodeSanti::registerModels, in its own file
//  so the node headers compile in parallel and an edited node header rebuilds
//  only its part. registerModels runs the parts in their old order.
//

#include "ofxSantiNodes.h"
#include "ofxOceanode.h"
#include "santiNodesTransportCompat.h"
#include "beatMeasures.h"
#ifdef OFXOCEANODE_USE_OSC
#include "BPMControl.h"
#endif
#include "cycleCount.h"
#include "currentTime.h"
#include "deltaTime.h"
#include "divmult2ms.h"
#include "framerateControl.h"
#include "phasorSwing.h"
#include "rateLimiter.h"
#include "resetPhasor.h"
#include "buttonMatrix.h"
#include "euclideanPatterns.h"
#include "euclideanTicks.h"
#include "euclideanTicksPoly.h"
#include "gateDuration.h"
#include "markovVector.h"
#include "noteMatrix.h"
#include "pianoRoll.h"
#include "pianoRollLive.h"
#include "polySeq.h"
#include "probSeq.h"
#include "soloSequencer.h"
#include "soloSequencerGui.h"
#include "soloStepSequencer.h"
#include "vectorSequencer.h"
#include "chancePass.h"
#include "chanceWeights.h"
#include "choose.h"
#include "randomSeries.h"
#include "randomValues.h"
#include "jazzWalk.h"
#include "melodicMutation.h"
#include "randomWalk.h"
#include "scramble.h"
#include "unrepeatedRandom.h"
#include "binaryEdgeDetector.h"
#include "boolToVoid.h"         // also contains: boolToFloat, floatToBool, floatToVoid
#include "debounce.h"
#include "edgeDetector.h"
#include "eventCounter.h"
#include "eventGate.h"
#include "flipflop.h"
#include "risingEdgeReindexer.h"
#include "sampleAndHold.h"
#include "trigger.h"
#include "vectorFire.h"
#include "voidBurst.h"
#include "voidCounter.h"
#include "voidToTick.h"
#include "voidSwitch.h"

namespace ofxOceanodeSanti{

void registerSequencingNodes(ofxOceanode &o)
{
    // ─────────────────────────────────────────────
    // TIMING & TRANSPORT
    // ─────────────────────────────────────────────
    o.registerModel<beatMeasures>("Santi/Timing");
#ifdef OFXOCEANODE_USE_OSC
    o.registerModel<BPMControl>("Santi/Timing");
#endif
    o.registerModel<cycleCount>("Santi/Timing");
    o.registerModel<currentTime>("Santi/Timing");
    o.registerModel<deltaTime>("Santi/Timing");
    o.registerModel<divmult2ms>("Santi/Timing");
    o.registerModel<framerateControl>("Santi/Timing");
    o.registerModel<phasorSwing>("Santi/Timing");
    o.registerModel<rateLimiter>("Santi/Timing");
    o.registerModel<resetPhasor>("Santi/Timing");

    // ─────────────────────────────────────────────
    // SEQUENCERS
    // ─────────────────────────────────────────────
    o.registerModel<buttonMatrix>("Santi/Sequencers");
    o.registerModel<euclideanPatterns>("Santi/Sequencers");
    o.registerModel<euclideanTicks>("Santi/Sequencers");
    o.registerModel<euclideanTicksPoly>("Santi/Sequencers");
    o.registerModel<markovVector>("Santi/Sequencers");
    o.registerModel<noteMatrix>("Santi/Sequencers");
    o.registerModel<pianoRoll>("Santi/Sequencers");
    o.registerModel<pianoRollLive>("Santi/Sequencers");
    o.registerModel<polySeq>("Santi/Sequencers");
    o.registerModel<probSeq>("Santi/Sequencers");
    o.registerModel<soloSequencer>("Santi/Sequencers");
    o.registerModel<soloSequencerGui>("Santi/Sequencers");
    o.registerModel<soloStepSequencer>("Santi/Sequencers");
    o.registerModel<vectorSequencer>("Santi/Sequencers");

    // ─────────────────────────────────────────────
    // CHANCE & RANDOMNESS
    // ─────────────────────────────────────────────
    o.registerModel<chancePass>("Santi/Chance");
    o.registerModel<chanceWeights>("Santi/Chance");
    o.registerModel<choose>("Santi/Chance");
    o.registerModel<randomSeries>("Santi/Chance");
    o.registerModel<randomValues>("Santi/Chance");
    o.registerModel<jazzWalk>("Santi/Chance");
    o.registerModel<melodicMutation>("Santi/Chance");
    o.registerModel<randomWalk>("Santi/Chance");
    o.registerModel<scramble>("Santi/Chance");
    o.registerModel<UnrepeatedRandom>("Santi/Chance");

    // ─────────────────────────────────────────────
    // EVENTS & TRIGGERS
    // ─────────────────────────────────────────────
    o.registerModel<binaryEdgeDetector>("Santi/Events");
    o.registerModel<boolToFloat>("Santi/Events");
    o.registerModel<boolToVoid>("Santi/Events");
    o.registerModel<debounce>("Santi/Events");
    o.registerModel<edgeDetector>("Santi/Events");
    o.registerModel<eventCounter>("Santi/Events");
    o.registerModel<eventGate>("Santi/Events");
    o.registerModel<gateDuration>("Santi/Events");
    o.registerModel<flipflop>("Santi/Events");
    o.registerModel<floatToBool>("Santi/Events");
    o.registerModel<floatToVoid>("Santi/Events");
    o.registerModel<risingEdgeReindexer>("Santi/Events");
    o.registerModel<sampleAndHold>("Santi/Events");
    o.registerModel<trigger>("Santi/Events");
    o.registerModel<vectorFire>("Santi/Events");
    o.registerModel<voidBurst>("Santi/Events");
    o.registerModel<voidCounter>("Santi/Events");
    o.registerModel<voidToTick>("Santi/Events");
	o.registerModel<voidSwitch>("Santi/Events");
}

}
