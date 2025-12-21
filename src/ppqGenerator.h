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

		// ---- Transport ----
		addParameter(play.set("Play", false));
		addParameter(reset.set("Reset"));

		// ---- Scrub ----
		addParameter(scrub.set("Scrub", 0.f, 0.f, 1.f));

		// ---- Tempo ----
		addParameter(bpm.set("BPM", 120.f, 1.f, 999.f));

		// ---- Meter / Structure ----
		addParameter(numerator.set("Numerator", 4, 1, 64));
		addParameter(denominator.set("Denominator", 4, 1, 64));
		addParameter(barLength.set("Bar Length", 4, 1, 1024));

		// ---- Outputs ----
		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addOutputParameter(phasor.set("Phasor", 0.f, 0.f, 1.f));

		// ---- Listeners ----
		listeners.push(reset.newListener([this]() {
			resetClock();
		}));

		listeners.push(scrub.newListener([this](float &s) {
			seekToPhase(s);
		}));

		resetClock();
	}

	void update(ofEventArgs &args) override {
		if(!play) return;

		float dt = ofGetLastFrameTime();
		if(dt <= 0.f) return;

		ppqAcc += dt * bpm.get() * 24.f / 60.f;
		updateOutputs();
	}

private:

	// ---------- Time / Meter ----------

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

	// ---------- Core logic ----------

	void updateOutputs() {
		int tot = totalTicks();
		if(tot <= 0) return;

		ppqAcc = std::max(0.f, ppqAcc);

		int newPpq = static_cast<int>(std::floor(ppqAcc));
		ppq24 = newPpq;

		int wrapped = newPpq % tot;
		if(wrapped < 0) wrapped += tot;

		phasor = float(wrapped) / float(tot);
	}

	void seekToPhase(float phase) {
		phase = ofClamp(phase, 0.f, 1.f);
		ppqAcc = phase * float(totalTicks());
		updateOutputs();
	}

	void resetClock() {
		ppqAcc = 0.f;
		ppq24 = 0;
		phasor = 0.f;
	}

	// ---------- State ----------

	float ppqAcc = 0.f;

	// ---------- Parameters ----------

	ofParameter<bool> play;
	ofParameter<void> reset;

	ofParameter<float> scrub;
	ofParameter<float> bpm;

	ofParameter<int> numerator;
	ofParameter<int> denominator;
	ofParameter<int> barLength;

	ofParameter<int>   ppq24;
	ofParameter<float> phasor;

	ofEventListeners listeners;
};

#endif /* ppqGenerator_h */
