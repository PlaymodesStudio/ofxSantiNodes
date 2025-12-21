#ifndef ppqMeter_h
#define ppqMeter_h

#include "ofxOceanodeNodeModel.h"
#include <climits>
#include <algorithm>

class ppqMeter : public ofxOceanodeNodeModel {
public:
	ppqMeter() : ofxOceanodeNodeModel("PPQ Meter") {}

	void setup() override {

		// Inputs
		addParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addParameter(numerator.set("Num", 4, 1, 64));
		addParameter(denominator.set("Den", 4, 1, 64));

		addParameter(barN.set("Bar N", 1, 1, 128));
		addParameter(modulo.set("%", 0, 0, INT_MAX));

		// Outputs
		addOutputParameter(barCount.set("BarCount", 0, 0, INT_MAX));
		addOutputParameter(barPhase.set("BarPh", 0.f, 0.f, 1.f));
		addOutputParameter(barTick.set("BarTick"));

		// Listeners
		listeners.push(ppq24.newListener([this](int &v) { compute(v); }));
		listeners.push(numerator.newListener([this](int &) { compute(ppq24.get()); }));
		listeners.push(denominator.newListener([this](int &) { compute(ppq24.get()); }));
		listeners.push(barN.newListener([this](int &) { compute(ppq24.get()); }));
		listeners.push(modulo.newListener([this](int &) { compute(ppq24.get()); }));

		compute(ppq24.get());
	}

private:

	int ticksPerBeat(int denom) const {
		switch(denom) {
			case 1:  return 96;
			case 2:  return 48;
			case 4:  return 24;
			case 8:  return 12;
			case 16: return 6;
			default:
				return int(96.0f / std::max(1, denom));
		}
	}

	void compute(int ppq) {

		int num   = std::max(1, numerator.get());
		int denom = std::max(1, denominator.get());
		int bars  = std::max(1, barN.get());
		int mod   = modulo.get();

		int tpb  = ticksPerBeat(denom);
		int tBar = tpb * num;
		int tGrp = tBar * bars;

		if(tGrp <= 0) return;

		// Absolute grouped bar index
		int grpIdx = ppq / tGrp;

		// Wrapped bar count (after modulo)
		int outBar = grpIdx;
		if(mod > 0) {
			outBar %= mod;
			if(outBar < 0) outBar += mod;
		}

		barCount = outBar;

		// Phase over grouped bar
		int r = ppq % tGrp;
		barPhase = float(r) / float(tGrp);

		// Tick on falling edge / wrap
		if(r == 0) {
			barTick.trigger();
		}
	}

	// ---- Parameters ----
	ofParameter<int> ppq24;
	ofParameter<int> numerator;
	ofParameter<int> denominator;
	ofParameter<int> barN;
	ofParameter<int> modulo;

	// ---- Outputs ----
	ofParameter<int> barCount;
	ofParameter<float> barPhase;
	ofParameter<void> barTick;

	ofEventListeners listeners;
};

#endif /* ppqMeter_h */
