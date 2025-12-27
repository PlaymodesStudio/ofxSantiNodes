#ifndef ppqBeats_h
#define ppqBeats_h

#include "ofxOceanodeNodeModel.h"
#include <cmath>
#include <climits>

class ppqBeats : public ofxOceanodeNodeModel {
public:
	ppqBeats() : ofxOceanodeNodeModel("PPQ Beat") {}

	void setup() override {

		// Input: absolute PPQ24 counter or beat transport
		addParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addParameter(beatTransport.set("Beat Transport", 0.f, 0.f, FLT_MAX));

		// Musical denominator: 4, 8, 16, 32...
		addParameter(th.set("Th", 4, 1, 128));

		addParameterDropdown(
			type,
			"Type",
			0, // default = Straight
			{
				"Straight",
				"Dotted",
				"Triplet"
			}
		);

		// Outputs
		addOutputParameter(count.set("Count", 0, 0, INT_MAX));
		addOutputParameter(phase.set("Phasor", 0.f, 0.f, 1.f));
		addOutputParameter(tick.set("Tick"));

		// Listeners
		listeners.push(ppq24.newListener([this](int &v) { compute(); }));
		listeners.push(beatTransport.newListener([this](float &v) { compute(); }));
		listeners.push(th.newListener([this](int &) { compute(); }));
		listeners.push(type.newListener([this](int &) { compute(); }));

		compute();
	}

private:

	int ticksPerUnit() const {
		if(th <= 0) return 0;

		// Whole note = 96 ticks in PPQ24 (4 * 24)
		float base = 96.0f / float(th);

		switch(type) {
		case 0: // Straight
			return int(std::round(base));
		case 1: // Dotted
			return int(std::round(base * 1.5f));
		case 2: // Triplet
			return int(std::round(base * 2.0f / 3.0f));
		default:
			return int(std::round(base));
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

		// Convert beats to ticks for calculation
		int ppqCalc = int(beats * 24.f);

		int ticks = ticksPerUnit();
		if(ticks <= 0) return;

		int r = ppqCalc % ticks;

		count = ppqCalc / ticks;
		phase = float(r) / float(ticks);

		// Tick on wrap / falling edge
		if(r == 0) {
			tick.trigger();
		}
	}

	// ---- Parameters ----
	ofParameter<int> ppq24;
	ofParameter<float> beatTransport;
	ofParameter<int> th;
	ofParameter<int> type;

	// ---- Outputs ----
	ofParameter<int> count;
	ofParameter<float> phase;
	ofParameter<void> tick;

	ofEventListeners listeners;
};

#endif /* ppqBeats_h */
