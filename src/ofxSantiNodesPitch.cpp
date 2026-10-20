//
//  ofxSantiNodesPitch.cpp
//
//  Matrix, pitch, harmony and voicing: part of ofxOceanodeSanti::registerModels, in its own file
//  so the node headers compile in parallel and an edited node header rebuilds
//  only its part. registerModels runs the parts in their old order.
//

#include "ofxSantiNodes.h"
#include "ofxOceanode.h"
#include "santiNodesTransportCompat.h"
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
#include "chordCypher.h"
#include "chordProgressions.h"
#include "fitNotesInRange.h"
#include "harmonyDetector.h"
#include "jazzStandards.h"
#include "pitchClassLoad.h"
#include "pitchGateFilter.h"
#include "risset.h"
#include "scalaTuning.h"
#include "harmonicPartials.h"
#include "harmonicSeries.h"
#include "intervalRatios.h"
#include "justChords.h"
#include "justIntonationAdapter.h"
#include "progression.h"
#include "bartokAxis.h"
#include "limitPolyphony.h"
#include "polyFill.h"
#include "polyphonicArpeggiator.h"
#include "schoenbergMatrix.h"
#include "voiceExpanding.h"
#include "voiceExpanding2.h"
#include "voiceStealing.h"

namespace ofxOceanodeSanti{

void registerPitchNodes(ofxOceanode &o)
{
    // ─────────────────────────────────────────────
    // MATRIX
    // ─────────────────────────────────────────────
    o.registerModel<matrixDisplay>("Santi/Matrix");
    o.registerModel<multiSliderGrid>("Santi/Matrix");
    o.registerModel<multiSliderMatrix>("Santi/Matrix");
    o.registerModel<pathwayGenerator>("Santi/Matrix");
    o.registerModel<vectorMatrixOffset>("Santi/Matrix");
    o.registerModel<vectorMatrixQuadrants>("Santi/Matrix");
    o.registerModel<vectorMatrixRadialSymmetry>("Santi/Matrix");
    o.registerModel<vectorMatrixReflect>("Santi/Matrix");
    o.registerModel<vectorMatrixResize>("Santi/Matrix");
    o.registerModel<vectorMatrixSymmetry>("Santi/Matrix");

    // ─────────────────────────────────────────────
    // PITCH
    // ─────────────────────────────────────────────
    o.registerModel<chordCypher>("Santi/Pitch");
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
    // retired 2026-10-18: replaced by SC ChordBox (ofxOceanodeSuperCollider; tools/migrate_chordsequence.py converts presets)
    // o.registerModel<chordSequence>("Santi/Pitch");
#endif
    o.registerModel<chordProgressions>("Santi/Pitch");
    o.registerModel<fitNotesInRange>("Santi/Pitch");
    o.registerModel<harmonyDetector>("Santi/Pitch");
    o.registerModel<jazzStandards>("Santi/Pitch");
    o.registerModel<pitchClassLoad>("Santi/Pitch");
    o.registerModel<pitchGateFilter>("Santi/Pitch");
    o.registerModel<risset>("Santi/Pitch");
    o.registerModel<scalaTuning>("Santi/Pitch");

    // ─────────────────────────────────────────────
    // HARMONY
    // ─────────────────────────────────────────────
    o.registerModel<harmonicPartials>("Santi/Harmony");
    o.registerModel<harmonicSeries>("Santi/Harmony");
    o.registerModel<intervalRatios>("Santi/Harmony");
    o.registerModel<justChords>("Santi/Harmony");
    o.registerModel<justIntonationAdapter>("Santi/Harmony");
    o.registerModel<progression>("Santi/Harmony");

    // ─────────────────────────────────────────────
    // VOICING & POLYPHONY
    // ─────────────────────────────────────────────
    o.registerModel<bartokAxis>("Santi/Voicing");
    o.registerModel<limitPolyphony>("Santi/Voicing");
    o.registerModel<polyFill>("Santi/Voicing");
    o.registerModel<polyphonicArpeggiator>("Santi/Voicing");
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
    // retired 2026-10-18: replaced by SC ArpeggiatorBox (ofxOceanodeSuperCollider; tools/migrate_polyarp_gui.py converts presets)
    // o.registerModel<polyphonicArpeggiatorGUI>("Santi/Voicing");
#endif
    o.registerModel<schoenbergMatrix>("Santi/Voicing");
    o.registerModel<voiceExpanding>("Santi/Voicing");
    o.registerModel<voiceExpanding2>("Santi/Voicing");
    o.registerModel<voiceStealing>("Santi/Voicing");
}

}
