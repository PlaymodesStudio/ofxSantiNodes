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
		addParameter(beatTransport.set("Beat Transport", 0.f, 0.f, FLT_MAX));
		addParameter(numerator.set("Num", 4, 1, 64));
		addParameter(denominator.set("Den", 4, 1, 64));

		addParameter(barN.set("Bar N", 1, 1, 128));
		addParameter(modulo.set("%", 0, 0, INT_MAX));

		// Outputs
		addOutputParameter(barCount.set("BarCount", 0, 0, INT_MAX));
		addOutputParameter(barPhase.set("BarPh", 0.f, 0.f, 1.f));
		addOutputParameter(barTick.set("BarTick"));

		// Listeners
		listeners.push(ppq24.newListener([this](int &v) { compute(); }));
		listeners.push(beatTransport.newListener([this](float &v) { compute(); }));
		listeners.push(numerator.newListener([this](int &) { compute(); }));
		listeners.push(denominator.newListener([this](int &) { compute(); }));
		listeners.push(barN.newListener([this](int &) { compute(); }));
		listeners.push(modulo.newListener([this](int &) { compute(); }));

		compute();
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

	void compute() {
		// Use beatTransport if available, otherwise fall back to PPQ24
		float beats = beatTransport.get();
		int ppq = ppq24.get();
		
		// Prefer beatTransport if it's non-zero or if ppq is zero
		if(beats <= 0.f && ppq > 0) {
			beats = ppq / 24.f;
		}

		int num   = std::max(1, numerator.get());
		int denom = std::max(1, denominator.get());
		int bars  = std::max(1, barN.get());
		int mod   = modulo.get();

		// Convert beats to ticks for calculation
		int ppqCalc = int(beats * 24.f);

		int tpb  = ticksPerBeat(denom);
		int tBar = tpb * num;
		int tGrp = tBar * bars;

		if(tGrp <= 0) return;

		// Absolute grouped bar index
		int grpIdx = ppqCalc / tGrp;

		// Wrapped bar count (after modulo)
		int outBar = grpIdx;
		if(mod > 0) {
			outBar %= mod;
			if(outBar < 0) outBar += mod;
		}

		barCount = outBar;

		// Phase over grouped bar
		int r = ppqCalc % tGrp;
		barPhase = float(r) / float(tGrp);

		// Tick on falling edge / wrap
		if(r == 0) {
			barTick.trigger();
		}
	}

	// ---- Parameters ----
	ofParameter<int> ppq24;
	ofParameter<float> beatTransport;
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
