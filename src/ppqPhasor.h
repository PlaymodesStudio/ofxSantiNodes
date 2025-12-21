#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>

class ppqPhasor : public ofxOceanodeNodeModel {
public:
	ppqPhasor() : ofxOceanodeNodeModel("PPQ Phasor") {}

	void setup() override {

		// ───────────── Inputs ─────────────

		// Absolute PPQ24 tick counter (from MIDI clock node)
		addParameter(ppq24.set("PPQ24", {0.0f}, {0.0f}, {FLT_MAX}));

		// Musical scaling
		addParameter(beatsMult.set("Beats Mult", {1.0f}, {0.0f}, {128.0f}));
		addParameter(beatsDiv.set("Beats Div", {1.0f}, {0.0001f}, {128.0f}));

		// Phase offset in cycles
		addParameter(initPhase.set("Phase Off", {0.0f}, {-10.0f}, {10.0f}));

		// Behaviour
		addParameter(loop.set("Loop", true));
		addParameter(reset.set("Reset", false));
		addParameter(fixednum.set("Fixed Cycles", 0, 0, 100000));

		// ───────────── Outputs ─────────────

		addOutputParameter(out.set("Out", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(cycleCountOut.set("Cycle Count", {0}, {0}, {INT_MAX}));

		// ───────────── State ─────────────

		basePPQ = 0.0f;

		listeners.push(reset.newListener([this](bool &b){
			if(b){
				// Snap phase origin to current PPQ
				basePPQ = getPPQ();
				std::fill(lastCycleCount.begin(), lastCycleCount.end(), 0);
				cycleCountOut = lastCycleCount;
				reset = false;
			}
		}));

		auto recompute = [this](){ compute(); };

		listeners.push(ppq24.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(beatsMult.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(beatsDiv.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(initPhase.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(loop.newListener([recompute](bool&){ recompute(); }));
		listeners.push(fixednum.newListener([recompute](int&){ recompute(); }));

		compute();
	}

private:
	// ───────────── Parameters ─────────────

	ofParameter<std::vector<float>> ppq24;
	ofParameter<std::vector<float>> beatsMult;
	ofParameter<std::vector<float>> beatsDiv;
	ofParameter<std::vector<float>> initPhase;
	ofParameter<bool> loop;
	ofParameter<bool> reset;
	ofParameter<int> fixednum;

	ofParameter<std::vector<float>> out;
	ofParameter<std::vector<int>> cycleCountOut;

	ofEventListeners listeners;

	// ───────────── Internal State ─────────────

	float basePPQ = 0.0f;
	std::vector<int> lastCycleCount;

	// ───────────── Utilities ─────────────

	static inline float frac(float x){
		return x - std::floor(x);
	}

	template<typename T>
	static inline T getIdx(const std::vector<T>& v, int i, T fallback){
		if(v.empty()) return fallback;
		if(i < 0) return v.front();
		if(i >= (int)v.size()) return v.back();
		return v[i];
	}

	float getPPQ() const {
		const auto& v = ppq24.get();
		return v.empty() ? 0.0f : v[0];
	}

	void resizeState(size_t n){
		if(n == 0) n = 1;

		if(lastCycleCount.size() != n){
			lastCycleCount.assign(n, 0);
			cycleCountOut = lastCycleCount;

			auto o = out.get();
			o.assign(n, 0.0f);
			out = o;
		}
	}

	// ───────────── Core Logic ─────────────

	void compute(){

		const float ppq = std::max(0.0f, getPPQ() - basePPQ);
		const float beatsAbs = ppq / 24.0f; // PPQ24 → quarter notes

		const auto& multV = beatsMult.get();
		const auto& divV  = beatsDiv.get();
		const auto& phV   = initPhase.get();

		const size_t n = std::max({multV.size(), divV.size(), phV.size(), (size_t)1});
		resizeState(n);

		std::vector<float> outV(n, 0.0f);
		std::vector<int> cycV(n, 0);

		const bool loopOn = loop.get();
		const int fixed = fixednum.get();

		for(size_t i = 0; i < n; ++i){

			const float mult = getIdx(multV, i, 1.0f);
			float div = getIdx(divV, i, 1.0f);
			if(div <= 0.0f) div = 0.0001f;

			const float cycles = beatsAbs * (mult / div);
			const int cyc = std::max(0, (int)std::floor(cycles));

			float ph = frac(cycles + getIdx(phV, i, 0.0f));

			// Fixed-length behaviour
			if(fixed > 0 && cyc >= fixed){
				ph = 0.0f;
			}

			// One-shot behaviour
			if(!loopOn && cyc >= 1){
				ph = 0.0f;
			}

			outV[i] = ph;
			cycV[i] = cyc;
		}

		out = outV;
		cycleCountOut = cycV;
		lastCycleCount = cycV;
	}
};
