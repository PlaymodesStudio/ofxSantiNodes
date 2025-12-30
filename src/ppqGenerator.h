#ifndef ppqGenerator_h
#define ppqGenerator_h

#include "ofxOceanodeNodeModel.h"
#include <cmath>
#include <climits>
#include <algorithm>

class ppqGenerator : public ofxOceanodeNodeModel {
public:
	ppqGenerator() : ofxOceanodeNodeModel("PPQ Generator") {}

	void setup() override {

		// ---- INPUTS ----
		addSeparator("INPUTS", ofColor(240, 240, 240));

		addParameter(play.set("Play", false));
		addParameter(reset.set("Reset"));
		addParameter(scrub.set("Scrub", 0.f, 0.f, 1.f));

		// ---- TEMPO ----
		addSeparator("TEMPO", ofColor(240, 240, 240));

		addParameter(bpm.set("BPM", 120.f, 1.f, 999.f));

		// ---- METER / STRUCTURE ----
		addSeparator("METER", ofColor(240, 240, 240));

		addParameter(numerator.set("Numerator", 4, 1, 64));
		addParameter(denominator.set("Denominator", 4, 1, 64));
		addParameter(barLength.set("Bar Length", 4, 1, 1024));

		// ---- OUTPUTS ----
		addSeparator("OUTPUTS", ofColor(240, 240, 240));

		addOutputParameter(playState.set("Play", false));
		playState.setSerializable(false);

		addOutputParameter(stopState.set("Stop", false));
		stopState.setSerializable(false);

		addOutputParameter(jumpTrig.set("Jump", false));
		jumpTrig.setSerializable(false);

		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		ppq24.setSerializable(false);

		addOutputParameter(ppq24f.set("PPQ 24f", 0.f, 0.f, FLT_MAX));
		ppq24f.setSerializable(false);

		addOutputParameter(beatTransport.set("Beat Transport", 0.f, 0.f, FLT_MAX));
		beatTransport.setSerializable(false);

		addOutputParameter(phasor.set("Phasor", 0.f, 0.f, 1.f));
		phasor.setSerializable(false);

		// ---- LISTENERS ----
		listeners.push(play.newListener([this](bool &p) {
			// Detect play state changes
			bool wasPlaying = playState.get();
			playState = p;
			stopState = !p;

			// Jump trigger on play start
			if(p && !wasPlaying) {
				jumpTrig = true;
			}
		}));

		listeners.push(reset.newListener([this]() {
			resetClock();
		}));

		listeners.push(scrub.newListener([this](float &s) {
			seekToPhase(s);
		}));

		resetClock();
	}

	void update(ofEventArgs &args) override {
		// Reset jump trigger at start of frame
		jumpTrig = false;

		if(!play) return;

		float dt = ofGetLastFrameTime();
		if(dt <= 0.f) return;

		ppqAcc += dt * bpm.get() * 24.f / 60.f;
		updateOutputs();
	}

private:

	// ---------- TIME / METER ----------

	int ticksPerBeat(int denom) const {
		switch(denom) {
		case 1:  return 96;
		case 2:  return 48;
		case 4:  return 24;
		case 8:  return 12;
		case 16: return 6;
		default:
			return int(96.f / std::max(1, denom));
		}
	}

	int totalTicks() const {
		int num   = std::max(1, numerator.get());
		int denom = std::max(1, denominator.get());
		int bars  = std::max(1, barLength.get());

		int tpb  = ticksPerBeat(denom);
		int tBar = tpb * num;

		return tBar * bars;
	}

	// ---------- CORE LOGIC ----------

	void updateOutputs() {
		int tot = totalTicks();
		if(tot <= 0) return;

		ppqAcc = std::max(0.f, ppqAcc);

		// Integer PPQ for compatibility
		int newPpq = static_cast<int>(std::floor(ppqAcc));
		ppq24 = newPpq;

		// High-precision floating-point PPQ
		ppq24f = ppqAcc;

		// Beat transport
		beatTransport = ppqAcc / 24.f;

		// Calculate phasor
		int wrapped = newPpq % tot;
		if(wrapped < 0) wrapped += tot;

		phasor = float(wrapped) / float(tot);
	}

	void seekToPhase(float phase) {
		phase = ofClamp(phase, 0.f, 1.f);
		ppqAcc = phase * float(totalTicks());
		updateOutputs();

		// Scrubbing triggers a jump
		jumpTrig = true;
	}

	void resetClock() {
		ppqAcc = 0.f;
		ppq24 = 0;
		ppq24f = 0.f;
		beatTransport = 0.f;
		phasor = 0.f;
		playState = false;
		stopState = true;

		// Reset triggers a jump
		jumpTrig = true;
	}

	// ---------- STATE ----------

	float ppqAcc = 0.f;

	// ---------- PARAMETERS ----------

	ofParameter<bool> play;
	ofParameter<void> reset;

	ofParameter<float> scrub;
	ofParameter<float> bpm;

	ofParameter<int> numerator;
	ofParameter<int> denominator;
	ofParameter<int> barLength;

	ofParameter<bool> playState;
	ofParameter<bool> stopState;
	ofParameter<bool> jumpTrig;

	ofParameter<int>   ppq24;
	ofParameter<float> ppq24f;
	ofParameter<float> beatTransport;
	ofParameter<float> phasor;

	ofEventListeners listeners;
};

#endif /* ppqGenerator_h */

