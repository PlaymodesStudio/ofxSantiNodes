#include "ofxSantiNodes.h"
#include "ofxOceanode.h"

// ─────────────────────────────────────────────
// TIMING & TRANSPORT
// ─────────────────────────────────────────────
#include "beatMeasures.h"
#include "BPMControl.h"
#include "cycleCount.h"
#include "deltaTime.h"
#include "divmult2ms.h"
#include "framerateControl.h"
#include "phasorSwing.h"
#include "rateLimiter.h"
#include "resetPhasor.h"

// ─────────────────────────────────────────────
// TIMELINE (PPQ / DAW transport)
// ─────────────────────────────────────────────
#include "curveTrack.h"
#include "gateTrack.h"
#include "midiClockTransport.h"
#include "midiNoteQuantizer.h"
#include "pianoRollTrack.h"
#include "ppqBeats.h"
#include "ppqGenerator.h"
#include "ppqMeter.h"
#include "ppqPhasor.h"
#include "ppqTimeline.h"
#include "reaperOscTransport.h"
#include "transportQuantizer.h"
#include "valueTrack.h"

// ─────────────────────────────────────────────
// SEQUENCERS
// ─────────────────────────────────────────────
#include "buttonMatrix.h"
#include "euclideanPatterns.h"
#include "euclideanTicks.h"
#include "euclideanTicksPoly.h"
#include "gateDuration.h"
#include "markovVector.h"
#include "noteMatrix.h"
#include "pianoRoll.h"
#include "polySeq.h"
#include "probSeq.h"
#include "soloSequencer.h"
#include "soloStepSequencer.h"
#include "vectorSequencer.h"

// ─────────────────────────────────────────────
// CHANCE & RANDOMNESS
// ─────────────────────────────────────────────
#include "chancePass.h"
#include "chanceWeights.h"
#include "choose.h"
#include "randomSeries.h"
#include "randomValues.h"
#include "randomWalk.h"
#include "scramble.h"
#include "unrepeatedRandom.h"

// ─────────────────────────────────────────────
// EVENTS & TRIGGERS
// ─────────────────────────────────────────────
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
#include "voidCounter.h"
#include "voidToTick.h"

// ─────────────────────────────────────────────
// MATH & DSP
// ─────────────────────────────────────────────
#include "circularCrossfade.h"
#include "derivative.h"
#include "Fold.h"
#include "formula.h"
#include "histogram2.h"
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

// ─────────────────────────────────────────────
// VECTORS
// ─────────────────────────────────────────────
#include "binPermute.h"
#include "circularValueEaser.h"
#include "dataBuffer.h"
#include "dataBufferFeedbackMs.h"
#include "distribute.h"
#include "duplicator.h"
#include "filterDuplicates.h"
#include "generativeGrid2.h"
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
#include "splitRoute.h"
#include "valueIndex.h"
#include "valuesChanged.h"
#include "vecFilter.h"
#include "vectorExtract.h"
#include "vectorFeedback.h"
#include "vectorFile.h"
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
#include "vectorTimer.h"
#include "vectorToCoordinates.h"

// ─────────────────────────────────────────────
// MATRIX
// ─────────────────────────────────────────────
#include "matrixDisplay.h"
#include "multiSliderGrid.h"
#include "multiSliderMatrix.h"
#include "pathwayGenerator.h"
#include "vectorMatrixOffset.h"
#include "vectorMatrixQuadrants.h"
#include "vectorMatrixRadialSymmetry.h"
#include "vectorMatrixReflect.h"
#include "vectorMatrixResize.h"
#include "vectorMatrixSymmetry.h"

// ─────────────────────────────────────────────
// VECTOR GRAPHICS
// ─────────────────────────────────────────────
#include "barMaker.h"
#include "generativeGrid.h"
#include "pathMaker.h"
#include "trimGroupPaths.h"
#include "trimPathSequential.h"

// ─────────────────────────────────────────────
// PITCH
// ─────────────────────────────────────────────
#include "chordCypher.h"
#include "chordProgressions.h"
#include "fitNotesInRange.h"
#include "harmonyDetector.h"
#include "jazzStandards.h"
#include "scalaTuning.h"

// ─────────────────────────────────────────────
// HARMONY
// ─────────────────────────────────────────────
#include "harmonicPartials.h"
#include "harmonicSeries.h"
#include "intervalRatios.h"
#include "justChords.h"
#include "justIntonationAdapter.h"
#include "progression.h"

// ─────────────────────────────────────────────
// VOICING & POLYPHONY
// ─────────────────────────────────────────────
#include "bartokAxis.h"
#include "limitPolyphony.h"
#include "polyFill.h"
#include "polyphonicArpeggiator.h"
#include "schoenbergMatrix.h"
#include "voiceExpanding.h"
#include "voiceExpanding2.h"
#include "voiceStealing.h"

// ─────────────────────────────────────────────
// COLOR
// ─────────────────────────────────────────────
#include "colorToVector.h"      // also contains: vectorToColor
#include "hue2rgb.h"            // class is: hsv2rgb
#include "rgb2rgbw.h"
#include "rgbw2rgb.h"
#include "vectorColorGradient.h"

// ─────────────────────────────────────────────
// TEXTURES
// ─────────────────────────────────────────────
#include "cameraInput.h"
#include "pixelStretch.h"
#include "spectrogramShift.h"
#include "textureFlip.h"
#include "textureSnapshot.h"
#include "texUniForms.h"

// ─────────────────────────────────────────────
// STRINGS
// ─────────────────────────────────────────────
#include "append.h"
#include "csvStrings.h"
#include "filenameExtractor.h"
#include "ftos.h"
#include "prepend.h"
#include "probabilityDropdownList.h"
#include "string2float.h"
#include "stringBox.h"
#include "stringComparator.h"
#include "stringSwitch.h"
#include "stringVector.h"
#include "txtReader.h"

// ─────────────────────────────────────────────
// CONVERSIONS
// ─────────────────────────────────────────────
#include "conversions.h"
#include "csv2vector.h"

// ─────────────────────────────────────────────
// GUI
// ─────────────────────────────────────────────
#include "circularSpeakerScheme.h"
#include "indexMonitor.h"
#include "multiStateVector.h"   // class is: multistateVector
#include "padXY.h"
#include "pianoKeyboard.h"

// ─────────────────────────────────────────────
// AUDIO UTILS
// ─────────────────────────────────────────────
#include "dbap.h"
#include "envelopeGenerator2.h"
#include "equalLoudness.h"      // class is: EqualLoudness

// ─────────────────────────────────────────────
// SNAPSHOTS
// ─────────────────────────────────────────────
#include "globalSnapshots.h"
#include "snapshotClient.h"
#include "snapshotServer.h"

// ─────────────────────────────────────────────
// OSC
// ─────────────────────────────────────────────
#include "multiOscSender.h"

// ─────────────────────────────────────────────
// MIDI
// ─────────────────────────────────────────────
#include "rotoControlConfig.h"

// ─────────────────────────────────────────────
// TTS (Text-to-Speech)
// ─────────────────────────────────────────────
#include "catotron.h"           // class is: Catotron
#include "OpenAITTS.h"
#include "TTS.h"

// ─────────────────────────────────────────────
// GENERAL / UTILITY
// ─────────────────────────────────────────────
#include "change.h"
#include "counterReset.h"
#include "countNumber.h"
#include "frameGate.h"
#include "increment.h"
#include "ocurrence.h"          // class is: Ocurrence
#include "presetLoadTrigger.h"
#include "ramp.h"               // class is: rampTrigger
#include "shell.h"

// ─────────────────────────────────────────────
// THALASTASI
// ─────────────────────────────────────────────
#include "MPGeneTable.h"        // class is: geneTable
#include "table.h"
#include "tableRowId.h"
#include "verticalProfileTable.h"  // class is: verticalProfile

// ─────────────────────────────────────────────
// PORTAL SELECTORS
// ─────────────────────────────────────────────
#include "portalSelector.h"


namespace ofxOceanodeSanti{

void registerModels(ofxOceanode *o)
{
    // ─────────────────────────────────────────────
    // TIMING & TRANSPORT
    // ─────────────────────────────────────────────
    o->registerModel<beatMeasures>("Santi/Timing");
    o->registerModel<BPMControl>("Santi/Timing");
    o->registerModel<cycleCount>("Santi/Timing");
    o->registerModel<deltaTime>("Santi/Timing");
    o->registerModel<divmult2ms>("Santi/Timing");
    o->registerModel<framerateControl>("Santi/Timing");
    o->registerModel<phasorSwing>("Santi/Timing");
    o->registerModel<rateLimiter>("Santi/Timing");
    o->registerModel<resetPhasor>("Santi/Timing");

    // ─────────────────────────────────────────────
    // TIMELINE (PPQ / DAW transport)
    // ─────────────────────────────────────────────
    o->registerModel<curveTrack>("Santi/Timeline");
    o->registerModel<gateTrack>("Santi/Timeline");
    o->registerModel<midiClockTransport>("Santi/Timeline");
    o->registerModel<midiNoteQuantizer>("Santi/Timeline");
    o->registerModel<pianoRollTrack>("Santi/Timeline");
    o->registerModel<ppqBeats>("Santi/Timeline");
    o->registerModel<ppqGenerator>("Santi/Timeline");
    o->registerModel<ppqMeter>("Santi/Timeline");
    o->registerModel<ppqPhasor>("Santi/Timeline");
    o->registerModel<ppqTimeline>("Santi/Timeline");
    o->registerModel<reaperOscTransport>("Santi/Timeline");
    o->registerModel<transportQuantizer>("Santi/Timeline");
    o->registerModel<valueTrack>("Santi/Timeline");

    // ─────────────────────────────────────────────
    // SEQUENCERS
    // ─────────────────────────────────────────────
    o->registerModel<buttonMatrix>("Santi/Sequencers");
    o->registerModel<euclideanPatterns>("Santi/Sequencers");
    o->registerModel<euclideanTicks>("Santi/Sequencers");
    o->registerModel<euclideanTicksPoly>("Santi/Sequencers");
    o->registerModel<gateDuration>("Santi/Sequencers");
    o->registerModel<markovVector>("Santi/Sequencers");
    o->registerModel<noteMatrix>("Santi/Sequencers");
    o->registerModel<pianoRoll>("Santi/Sequencers");
    o->registerModel<polySeq>("Santi/Sequencers");
    o->registerModel<probSeq>("Santi/Sequencers");
    o->registerModel<soloSequencer>("Santi/Sequencers");
    o->registerModel<soloStepSequencer>("Santi/Sequencers");
    o->registerModel<vectorSequencer>("Santi/Sequencers");

    // ─────────────────────────────────────────────
    // CHANCE & RANDOMNESS
    // ─────────────────────────────────────────────
    o->registerModel<chancePass>("Santi/Chance");
    o->registerModel<chanceWeights>("Santi/Chance");
    o->registerModel<choose>("Santi/Chance");
    o->registerModel<randomSeries>("Santi/Chance");
    o->registerModel<randomValues>("Santi/Chance");
    o->registerModel<randomWalk>("Santi/Chance");
    o->registerModel<scramble>("Santi/Chance");
    o->registerModel<UnrepeatedRandom>("Santi/Chance");

    // ─────────────────────────────────────────────
    // EVENTS & TRIGGERS
    // ─────────────────────────────────────────────
    o->registerModel<binaryEdgeDetector>("Santi/Events");
    o->registerModel<boolToFloat>("Santi/Events");
    o->registerModel<boolToVoid>("Santi/Events");
    o->registerModel<debounce>("Santi/Events");
    o->registerModel<edgeDetector>("Santi/Events");
    o->registerModel<eventCounter>("Santi/Events");
    o->registerModel<eventGate>("Santi/Events");
    o->registerModel<flipflop>("Santi/Events");
    o->registerModel<floatToBool>("Santi/Events");
    o->registerModel<floatToVoid>("Santi/Events");
    o->registerModel<risingEdgeReindexer>("Santi/Events");
    o->registerModel<sampleAndHold>("Santi/Events");
    o->registerModel<trigger>("Santi/Events");
    o->registerModel<vectorFire>("Santi/Events");
    o->registerModel<voidCounter>("Santi/Events");
    o->registerModel<voidToTick>("Santi/Events");

    // ─────────────────────────────────────────────
    // MATH & DSP
    // ─────────────────────────────────────────────
    o->registerModel<circularCrossfade>("Santi/Math");
    o->registerModel<derivative>("Santi/Math");
    o->registerModel<fold>("Santi/Math");
    o->registerModel<formula>("Santi/Math");
    o->registerModel<histogram2>("Santi/Math");
    o->registerModel<Logic>("Santi/Math");
    o->registerModel<metaballAnalyzer>("Santi/Math");
    o->registerModel<polygonArea>("Santi/Math");
    o->registerModel<polygonPerimeter>("Santi/Math");
    o->registerModel<quantize>("Santi/Math");
    o->registerModel<sigmoidCurve>("Santi/Math");
    o->registerModel<slope>("Santi/Math");
    o->registerModel<trigonometry>("Santi/Math");
    o->registerModel<vectorBlur>("Santi/Math");
    o->registerModel<vectorDeglitch>("Santi/Math");
    o->registerModel<vectorFold>("Santi/Math");
    o->registerModel<vectorInterpolation>("Santi/Math");
    o->registerModel<vectorSplitOnMinusOne>("Santi/Math");

    // ─────────────────────────────────────────────
    // VECTORS
    // ─────────────────────────────────────────────
    o->registerModel<binPermute>("Santi/Vectors");
    o->registerModel<circularValueEaser>("Santi/Vectors");
    o->registerModel<dataBuffer>("Santi/Vectors");
    o->registerModel<dataBufferFeedbackMs>("Santi/Vectors");
    o->registerModel<distribute>("Santi/Vectors");
    o->registerModel<duplicator>("Santi/Vectors");
    o->registerModel<filterDuplicates>("Santi/Vectors");
    o->registerModel<generativeGrid2>("Santi/Vectors");
    o->registerModel<ignoreZeros>("Santi/Vectors");
    o->registerModel<indexHighlight>("Santi/Vectors");
    o->registerModel<indexRouter>("Santi/Vectors");
    o->registerModel<merger>("Santi/Vectors");
    o->registerModel<mergeVoid>("Santi/Vectors");
    o->registerModel<order>("Santi/Vectors");
    o->registerModel<permutations>("Santi/Vectors");
    o->registerModel<radialIndexer>("Santi/Vectors");
    o->registerModel<segmentLength>("Santi/Vectors");
    o->registerModel<splitMinMax>("Santi/Vectors");
    o->registerModel<splitRoute>("Santi/Vectors");
    o->registerModel<trimGroupPaths>("Santi/Vectors");
    o->registerModel<valueIndex>("Santi/Vectors");
    o->registerModel<valuesChanged>("Santi/Vectors");
    o->registerModel<vecFilter>("Santi/Vectors");
    o->registerModel<vectorExtract>("Santi/Vectors");
    o->registerModel<vectorFeedback>("Santi/Vectors");
    o->registerModel<vectorFile>("Santi/Vectors");
    o->registerModel<vectorInverter>("Santi/Vectors");
    o->registerModel<vectorMorphology>("Santi/Vectors");
    o->registerModel<vectorMorphologyVV>("Santi/Vectors");
    o->registerModel<vectorOfVectorDisplay>("Santi/Vectors");
    o->registerModel<vectorOfVectorIndexedSampler>("Santi/Vectors");
    o->registerModel<vectorPointer>("Santi/Vectors");
    o->registerModel<vectorRegion>("Santi/Vectors");
    o->registerModel<vectorRegionVV>("Santi/Vectors");
    o->registerModel<vectorSampler>("Santi/Vectors");
    o->registerModel<vectorSetter>("Santi/Vectors");
    o->registerModel<split>("Santi/Vectors");
    o->registerModel<vectorStorage>("Santi/Vectors");
    o->registerModel<vectorTimer>("Santi/Vectors");
    o->registerModel<vectorToCoordinates>("Santi/Vectors");

    // ─────────────────────────────────────────────
    // MATRIX
    // ─────────────────────────────────────────────
    o->registerModel<matrixDisplay>("Santi/Matrix");
    o->registerModel<multiSliderGrid>("Santi/Matrix");
    o->registerModel<multiSliderMatrix>("Santi/Matrix");
    o->registerModel<pathwayGenerator>("Santi/Matrix");
    o->registerModel<vectorMatrixOffset>("Santi/Matrix");
    o->registerModel<vectorMatrixQuadrants>("Santi/Matrix");
    o->registerModel<vectorMatrixRadialSymmetry>("Santi/Matrix");
    o->registerModel<vectorMatrixReflect>("Santi/Matrix");
    o->registerModel<vectorMatrixResize>("Santi/Matrix");
    o->registerModel<vectorMatrixSymmetry>("Santi/Matrix");

    // ─────────────────────────────────────────────
    // VECTOR GRAPHICS
    // ─────────────────────────────────────────────
    o->registerModel<barMaker>("Santi/VectorGraphics");
    o->registerModel<generativeGrid>("Santi/VectorGraphics");
    o->registerModel<pathMaker>("Santi/VectorGraphics");
    o->registerModel<trimPathSequential>("Santi/VectorGraphics");

    // ─────────────────────────────────────────────
    // PITCH
    // ─────────────────────────────────────────────
    o->registerModel<chordCypher>("Santi/Pitch");
    o->registerModel<chordProgressions>("Santi/Pitch");
    o->registerModel<fitNotesInRange>("Santi/Pitch");
    o->registerModel<harmonyDetector>("Santi/Pitch");
    o->registerModel<jazzStandards>("Santi/Pitch");
    o->registerModel<scalaTuning>("Santi/Pitch");

    // ─────────────────────────────────────────────
    // HARMONY
    // ─────────────────────────────────────────────
    o->registerModel<harmonicPartials>("Santi/Harmony");
    o->registerModel<harmonicSeries>("Santi/Harmony");
    o->registerModel<intervalRatios>("Santi/Harmony");
    o->registerModel<justChords>("Santi/Harmony");
    o->registerModel<justIntonationAdapter>("Santi/Harmony");
    o->registerModel<progression>("Santi/Harmony");

    // ─────────────────────────────────────────────
    // VOICING & POLYPHONY
    // ─────────────────────────────────────────────
    o->registerModel<bartokAxis>("Santi/Voicing");
    o->registerModel<limitPolyphony>("Santi/Voicing");
    o->registerModel<polyFill>("Santi/Voicing");
    o->registerModel<polyphonicArpeggiator>("Santi/Voicing");
    o->registerModel<schoenbergMatrix>("Santi/Voicing");
    o->registerModel<voiceExpanding>("Santi/Voicing");
    o->registerModel<voiceExpanding2>("Santi/Voicing");
    o->registerModel<voiceStealing>("Santi/Voicing");

    // ─────────────────────────────────────────────
    // COLOR
    // ─────────────────────────────────────────────
    o->registerModel<colorToVector>("Santi/Color");
    o->registerModel<hsv2rgb>("Santi/Color");
    o->registerModel<rgb2rgbw>("Santi/Color");
    o->registerModel<rgbw2rgb>("Santi/Color");
    o->registerModel<vectorColorGradient>("Santi/Color");
    o->registerModel<vectorToColor>("Santi/Color");

    // ─────────────────────────────────────────────
    // TEXTURES
    // ─────────────────────────────────────────────
    o->registerModel<cameraInput>("Santi/Textures");
    o->registerModel<pixelStretch>("Santi/Textures");
    o->registerModel<spectrogramShift>("Santi/Textures");
    o->registerModel<textureFlip>("Santi/Textures");
    o->registerModel<textureSnapshot>("Santi/Textures");
    o->registerModel<texUniForms>("Santi/Textures");

    // ─────────────────────────────────────────────
    // STRINGS
    // ─────────────────────────────────────────────
    o->registerModel<append>("Santi/Strings");
    o->registerModel<csvStrings>("Santi/Strings");
    o->registerModel<filenameExtractor>("Santi/Strings");
    o->registerModel<ftos>("Santi/Strings");
    o->registerModel<prepend>("Santi/Strings");
    o->registerModel<probabilityDropdownList>("Santi/Strings");
    o->registerModel<string2float>("Santi/Strings");
    o->registerModel<stringBox>("Santi/Strings");
    o->registerModel<stringComparator>("Santi/Strings");
    o->registerModel<stringSwitch>("Santi/Strings");
    o->registerModel<stringVector>("Santi/Strings");
    o->registerModel<txtReader>("Santi/Strings");

    // ─────────────────────────────────────────────
    // CONVERSIONS
    // ─────────────────────────────────────────────
    o->registerModel<conversions>("Santi/Conversions");
    o->registerModel<csv2vector>("Santi/Conversions");

    // ─────────────────────────────────────────────
    // GUI
    // ─────────────────────────────────────────────
    o->registerModel<circularSpeakerScheme>("Santi/GUI");
    o->registerModel<indexMonitor>("Santi/GUI");
    o->registerModel<multistateVector>("Santi/GUI");
    o->registerModel<padXY>("Santi/GUI");
    o->registerModel<pianoKeyboard>("Santi/GUI");
//  o->registerModel<toggle>("Santi/GUI");
//  o->registerModel<button>("Santi/GUI");
//  o->registerModel<slider>("Santi/GUI");
//  o->registerModel<multislider>("Santi/GUI");
//  o->registerModel<multitoggle>("Santi/GUI");
//  o->registerModel<value>("Santi/GUI");
//  o->registerModel<rangedSlider>("Santi/GUI");

    // ─────────────────────────────────────────────
    // AUDIO UTILS
    // ─────────────────────────────────────────────
    o->registerModel<dbap>("Santi/AudioUtils");
    o->registerModel<envelopeGenerator2>("Santi/AudioUtils");
    o->registerModel<EqualLoudness>("Santi/AudioUtils");

    // ─────────────────────────────────────────────
    // SNAPSHOTS
    // ─────────────────────────────────────────────
    o->registerModel<globalSnapshots>("Santi/Snapshots");
    o->registerModel<snapshotClient>("Santi/Snapshots");
    o->registerModel<snapshotServer>("Santi/Snapshots");

    // ─────────────────────────────────────────────
    // OSC
    // ─────────────────────────────────────────────
    o->registerModel<multiOscSender>("Santi/OSC");

    // ─────────────────────────────────────────────
    // MIDI
    // ─────────────────────────────────────────────
    o->registerModel<rotoControlConfig>("Santi/MIDI");

    // ─────────────────────────────────────────────
    // TTS (Text-to-Speech)
    // ─────────────────────────────────────────────
    o->registerModel<Catotron>("Santi/TTS");
    o->registerModel<OpenAITTS>("Santi/TTS");
    o->registerModel<TTS>("Santi/TTS");

    // ─────────────────────────────────────────────
    // GENERAL / UTILITY
    // ─────────────────────────────────────────────
    o->registerModel<change>("Santi/General");
    o->registerModel<counterReset>("Santi/General");
    o->registerModel<countNumber>("Santi/General");
    o->registerModel<frameGate>("Santi/General");
    o->registerModel<increment>("Santi/General");
    o->registerModel<Ocurrence>("Santi/General");
    o->registerModel<presetLoadTrigger>("Santi/General");
    o->registerModel<rampTrigger>("Santi/General");
    o->registerModel<shell>("Santi/General");

    // ─────────────────────────────────────────────
    // THALASTASI
    // ─────────────────────────────────────────────
    o->registerModel<geneTable>("Santi/Thalastasi");
    o->registerModel<table>("Santi/Thalastasi");
    o->registerModel<tableRowId>("Santi/Thalastasi");
    o->registerModel<verticalProfile>("Santi/Thalastasi");

    // ─────────────────────────────────────────────
    // PORTAL SELECTORS
    // ─────────────────────────────────────────────
    o->registerModel<portalSelector<bool>>("Santi/PortalSelectors", "b", false);
    o->registerModel<portalSelector<char>>("Santi/PortalSelectors", "c", ' ');
    o->registerModel<portalSelector<float>>("Santi/PortalSelectors", "f", 0.0f);
    o->registerModel<portalSelector<int>>("Santi/PortalSelectors", "i", 0);
    o->registerModel<portalSelector<ofColor>>("Santi/PortalSelectors", "color", ofColor::black);
    o->registerModel<portalSelector<ofFloatColor>>("Santi/PortalSelectors", "color_f", ofFloatColor::black);
    o->registerModel<portalSelector<ofTexture*>>("Santi/PortalSelectors", "texture", nullptr);
    o->registerModel<portalSelector<string>>("Santi/PortalSelectors", "s", "");
    o->registerModel<portalSelector<Timestamp>>("Santi/PortalSelectors", "timestamp", Timestamp());
    o->registerModel<portalSelector<vector<float>>>("Santi/PortalSelectors", "v_f", 0.0f);
    o->registerModel<portalSelector<vector<int>>>("Santi/PortalSelectors", "v_i", 0);
    o->registerModel<portalSelector<vector<string>>>("Santi/PortalSelectors", "v_s", string(""));
    o->registerModel<portalSelector<void>>("Santi/PortalSelectors", "v");
    //o->registerModel<portalSelector<nodePort>>("Santi/PortalSelectors", "scbus", nodePort());
}
}
