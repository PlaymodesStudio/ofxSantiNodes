#ifndef midiClockTransport_h
#define midiClockTransport_h

#include "ofxOceanodeNodeModel.h"
#include "ofxMidi.h"

class midiClockTransport :
	public ofxOceanodeNodeModel,
	public ofxMidiListener {

public:
	midiClockTransport()
	: ofxOceanodeNodeModel("MIDI Clock Transport") {}

	~midiClockTransport() {
		stopMidi();
	}

	void setup() {
		/* -------- Inputs -------- */
		addParameterDropdown(midiPort, "Port", 0, midiIn.getInPortList());
		addParameter(enable.set("Enable", false));
		addParameter(listPorts.set("List Ports"));

		/* -------- Transport outputs -------- */
		addOutputParameter(playState.set("Play", false));
		addOutputParameter(stopState.set("Stop", false));

		/* -------- MIDI Clock outputs -------- */
		addOutputParameter(beat.set("Beat", 0.f, 0.f, FLT_MAX));
		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addOutputParameter(timeSeconds.set("Time(s)", 0.f, 0.f, FLT_MAX));
		addOutputParameter(bpm.set("BPM", 120.f, 1.f, 999.f));

		/* -------- Listeners -------- */
		listeners.push(midiPort.newListener([this](int &device){
			restartMidi(device);
		}));

		listeners.push(enable.newListener([this](bool &e){
			e ? startMidi() : stopMidi();
		}));

		listeners.push(listPorts.newListener([this](){
			midiIn.listInPorts();
		}));

		resetClock();
	}

	/* ================= MIDI CALLBACK ================= */

	void newMidiMessage(ofxMidiMessage &msg) override {

		if(msg.status == MIDI_TIME_CLOCK) {
			uint64_t now = ofGetElapsedTimeMillis();

			tickCount++;
			ppq24 = tickCount;

			float currentBeat = tickCount / 24.f;
			beat = currentBeat;

			if(lastClockTime > 0) {
				uint64_t dt = now - lastClockTime;
				if(dt > 0 && dt < 1000) {
					float instBpm = 60000.f / (dt * 24.f);
					bpm = bpm * 0.95f + instBpm * 0.05f;
				}
			}

			timeSeconds = (currentBeat / bpm) * 60.f;
			lastClockTime = now;

			playState = true;
			stopState = false;
		}

		else if(msg.status == MIDI_START) {
			resetClock();
			playState = true;
			stopState = false;
		}

		else if(msg.status == MIDI_CONTINUE) {
			playState = true;
			stopState = false;
		}

		else if(msg.status == MIDI_STOP) {
			playState = false;
			stopState = true;
		}
	}

private:
	/* ================= MIDI MANAGEMENT ================= */

	void startMidi() {
		if(midiIn.isOpen()) return;

		midiIn.openPort(midiPort);

		// CRITICAL: allow timing messages (MIDI Clock)
		midiIn.ignoreTypes(
			false, // sysex
			false, // timing
			false  // active sensing
		);

		midiIn.addListener(this);

		ofLogNotice("midiClockTransport")
			<< "Opened MIDI port: " << midiPort.get();
	}

	void stopMidi() {
		if(midiIn.isOpen()) {
			midiIn.removeListener(this);
			midiIn.closePort();
		}
	}

	void restartMidi(int device) {
		stopMidi();
		if(enable) startMidi();
	}

	/* ================= CLOCK STATE ================= */

	void resetClock() {
		tickCount = 0;
		lastClockTime = 0;
		beat = 0.f;
		ppq24 = 0;
		timeSeconds = 0.f;
	}

	/* -------- MIDI -------- */
	ofxMidiIn midiIn;

	/* -------- State -------- */
	uint64_t lastClockTime = 0;
	int tickCount = 0;

	/* -------- Parameters -------- */
	ofParameter<int> midiPort;
	ofParameter<bool> enable;
	ofParameter<void> listPorts;

	ofParameter<bool> playState;
	ofParameter<bool> stopState;

	ofParameter<float> beat;
	ofParameter<int> ppq24;
	ofParameter<float> timeSeconds;
	ofParameter<float> bpm;

	ofEventListeners listeners;
};

#endif /* midiClockTransport_h */
