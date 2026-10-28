//
//  ofxSantiNodesUtility.cpp
//
//  Color, textures, strings, conversions, GUI, audio, snapshots, MIDI and utilities: part of ofxOceanodeSanti::registerModels, in its own file
//  so the node headers compile in parallel and an edited node header rebuilds
//  only its part. registerModels runs the parts in their old order.
//

#include "ofxSantiNodes.h"
#include "ofxOceanode.h"
#include "ofxOceanodeTransport.h"
#include "histogramVector.h"
#include "colorToVector.h"      // also contains: vectorToColor
#include "hue2rgb.h"            // class is: hsv2rgb
#include "rgb2rgbw.h"
#include "rgbw2rgb.h"
#include "vectorColorGradient.h"
#include "pixelStretch.h"
#include "spectrogramShift.h"
#include "append.h"
#include "csvStrings.h"
#include "filenameExtractor.h"
#include "ftos.h"
#include "prepend.h"
#include "string2float.h"
#include "stringBox.h"
#include "stringComparator.h"
#include "stringSwitch.h"
#include "stringVector.h"
#include "txtReader.h"
#include "conversions.h"
#include "csv2vector.h"
#include "circularSpeakerScheme.h"
#include "indexMonitor.h"
#include "multiStateVector.h"   // class is: multistateVector
#include "multiToggleNode.h"
#include "padXY.h"
#include "pianoKeyboard.h"
#include "toggle.h"
#include "slider.h"
#include "button.h"
#include "multislider.h"
#include "multitoggle.h"
#include "value.h"
#include "intSlider.h"
#include "intValue.h"
#include "rangedSlider.h"
#include "dbap.h"
#include "envelopeGenerator2.h"
#include "equalLoudness.h"      // class is: EqualLoudness
#include "globalSnapshots.h"
#ifdef OFXOCEANODE_USE_MIDI
#include "rotoControlConfig.h"
#endif
#include "midiKeyboardEmulator.h"
#include "change.h"
#include "counterReset.h"
#include "countNumber.h"
#include "frameGate.h"
#include "increment.h"
#include "ocurrence.h"          // class is: Ocurrence
#include "ramp.h"               // class is: rampTrigger
#include "shell.h"
#include "keystroke.h"
#include "dataDelay.h"

namespace ofxOceanodeSanti{

void registerUtilityNodes(ofxOceanode &o)
{
    // ─────────────────────────────────────────────
    // COLOR
    // ─────────────────────────────────────────────
    o.registerModel<colorToVector>("Santi/Color");
    o.registerModel<hsv2rgb>("Santi/Color");
    o.registerModel<rgb2rgbw>("Santi/Color");
    o.registerModel<rgbw2rgb>("Santi/Color");
    o.registerModel<vectorColorGradient>("Santi/Color");
    o.registerModel<vectorToColor>("Santi/Color");

    // ─────────────────────────────────────────────
    // TEXTURES
    // ─────────────────────────────────────────────
    o.registerModel<pixelStretch>("Santi/Textures");
    o.registerModel<histogramVector>("Santi/Textures");
    o.registerModel<spectrogramShift>("Santi/Textures");

    // ─────────────────────────────────────────────
    // STRINGS
    // ─────────────────────────────────────────────
    o.registerModel<append>("Santi/Strings");
    o.registerModel<csvStrings>("Santi/Strings");
    o.registerModel<filenameExtractor>("Santi/Strings");
    o.registerModel<ftos>("Santi/Strings");
    o.registerModel<prepend>("Santi/Strings");
    o.registerModel<string2float>("Santi/Strings");
    o.registerModel<stringBox>("Santi/Strings");
    o.registerModel<stringComparator>("Santi/Strings");
    o.registerModel<stringSwitch>("Santi/Strings");
    o.registerModel<stringVector>("Santi/Strings");
    o.registerModel<txtReader>("Santi/Strings");

    // ─────────────────────────────────────────────
    // CONVERSIONS
    // ─────────────────────────────────────────────
    o.registerModel<conversions>("Santi/Conversions");
    o.registerModel<csv2vector>("Santi/Conversions");

    // ─────────────────────────────────────────────
    // GUI
    // ─────────────────────────────────────────────
    o.registerModel<circularSpeakerScheme>("Santi/GUI");
    o.registerModel<indexMonitor>("Santi/GUI");
    o.registerModel<multistateVector>("Santi/GUI");
    o.registerModel<multiToggleNode>("Santi/GUI");
    o.registerModel<padXY>("Santi/GUI");
    o.registerModel<pianoKeyboard>("Santi/GUI");
	o.registerModel<toggle>("Santi/GUI");
	o.registerModel<button>("Santi/GUI");
	o.registerModel<slider>("Santi/GUI");
	o.registerModel<multislider>("Santi/GUI");
	o.registerModel<multitoggle>("Santi/GUI");
	o.registerModel<value>("Santi/GUI");
	o.registerModel<intSlider>("Santi/GUI");
	o.registerModel<intValue>("Santi/GUI");
	o.registerModel<rangedSlider>("Santi/GUI");

    // ─────────────────────────────────────────────
    // AUDIO UTILS
    // ─────────────────────────────────────────────
    o.registerModel<dbap>("Santi/AudioUtils");
    o.registerModel<envelopeGenerator2>("Santi/AudioUtils");
    o.registerModel<EqualLoudness>("Santi/AudioUtils");

    // ─────────────────────────────────────────────
    // SNAPSHOTS
    // ─────────────────────────────────────────────
    o.registerModel<globalSnapshots>("Santi/Snapshots", ofxOceanodeNodeRegistrationFlags::Sink);

    // ─────────────────────────────────────────────
    // MIDI
    // ─────────────────────────────────────────────
    o.registerModel<midiKeyboardEmulator>("Santi/MIDI");
#ifdef OFXOCEANODE_USE_MIDI
    o.registerModel<rotoControlConfig>("Santi/MIDI", ofxOceanodeNodeRegistrationFlags::Sink);
#endif

    // ─────────────────────────────────────────────
    // GENERAL / UTILITY
    // ─────────────────────────────────────────────
    o.registerModel<change>("Santi/General");
    o.registerModel<counterReset>("Santi/General");
    o.registerModel<countNumber>("Santi/General");
    o.registerModel<frameGate>("Santi/General");
    o.registerModel<increment>("Santi/General");
    o.registerModel<Ocurrence>("Santi/General");
    o.registerModel<rampTrigger>("Santi/General");
    o.registerModel<shell>("Santi/General");
	o.registerModel<keystroke>("Santi/General");
	o.registerModel<dataDelay>("Santi/General");
}

}
