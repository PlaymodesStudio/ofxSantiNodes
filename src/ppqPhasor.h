#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>

class ppqPhasor : public ofxOceanodeNodeModel {
public:
	ppqPhasor() : ofxOceanodeNodeModel("PPQ Phasor") {
		description =
			"Phase-locked ramp synchronized to MIDI clock (PPQ24).\n"
			"Features frequency modulation (FM) with smooth phase-locking (PLL).\n"
			"Jump Trigger forces hard-sync for 3 frames.\n"
			"ResyncTime controls how fast the PLL corrects drift.";
	}
	
	void setup() override {
		
		// ───────────── Inputs ─────────────
		addSeparator("INPUTS", ofColor(240, 240, 240));
		
		// Transport
		addParameter(ppq24.set("PPQ24f", {0.0f}, {0.0f}, {FLT_MAX}));
		addParameter(beatTransport.set("BeatTransport", {0.0f}, {0.0f}, {FLT_MAX}));
		
		// Triggers
		addParameter(jump.set("Jump", {0}, {0}, {1}));
		addParameter(reset.set("Reset", 0, 0, 1)); // Global reset (int)
		
		// Musical Params
		addParameter(beats.set("Beats", {1.0f}, {0.0001f}, {128.0f}));
		addParameter(initPhase.set("PhaseOff", {0.0f}, {-10.0f}, {10.0f}));
		addParameter(fmMod.set("FM", {1.0f}, {0.0f}, {10.0f}));
		
		// ───────────── Behavior ─────────────
		addSeparator("BEHAVIOR", ofColor(240, 240, 240));
		
		addParameter(resyncTime.set("ResyncTime", 0.5f, 0.01f, 10.0f));
		addParameter(forwardPLL.set("ForwardPLL", 1, 0, 1)); // int
		addParameter(loop.set("Loop", 1, 0, 1));             // int
		addParameter(fixednum.set("FixedCycles", 0, 0, 100000));
		
		// ───────────── Outputs ─────────────
		addSeparator("OUTPUTS", ofColor(240, 240, 240));
		addOutputParameter(cycleCountOut.set("CycleCount", {0}, {0}, {INT_MAX}));
		addOutputParameter(trigger.set("Trigger", {0}, {0}, {1}));
		addOutputParameter(out.set("Out", {0.0f}, {0.0f}, {1.0f}));
		
		// ───────────── Listeners ─────────────
		
		// Global reset listener (reset internal base counters)
		listeners.push(reset.newListener([this](int &val){
			if(val > 0){
				basePPQ = getPPQ();
				baseBeats = getBeats();
				lastPPQ = getPPQ();
				lastBeats = getBeats();
				std::fill(lastCycleCount.begin(), lastCycleCount.end(), 0);
				std::fill(accumulatedPhase.begin(), accumulatedPhase.end(), 0.0f);
				std::fill(jumpCounters.begin(), jumpCounters.end(), 0);
				
				cycleCountOut = lastCycleCount;
				
				// Auto-reset the parameter back to 0 to act like a trigger
				reset.setWithoutEventNotifications(0);
			}
		}));
		
		auto recompute = [this](){ compute(); };
		
		listeners.push(ppq24.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(beatTransport.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(jump.newListener([recompute](std::vector<int>&){ recompute(); }));
		listeners.push(beats.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(initPhase.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(fmMod.newListener([recompute](std::vector<float>&){ recompute(); }));
		listeners.push(resyncTime.newListener([recompute](float&){ recompute(); }));
		listeners.push(forwardPLL.newListener([recompute](int&){ recompute(); }));
		listeners.push(loop.newListener([recompute](int&){ recompute(); }));
		listeners.push(fixednum.newListener([recompute](int&){ recompute(); }));
		
		compute();
	}
	
private:
	// ───────────── Parameters ─────────────
	ofParameter<std::vector<float>> ppq24;
	ofParameter<std::vector<float>> beatTransport;
	ofParameter<std::vector<int>> jump;
	ofParameter<int> reset;
	
	ofParameter<std::vector<float>> beats;
	ofParameter<std::vector<float>> initPhase;
	ofParameter<std::vector<float>> fmMod;
	
	ofParameter<float> resyncTime;
	ofParameter<int> forwardPLL;
	ofParameter<int> loop;
	ofParameter<int> fixednum;
	
	ofParameter<std::vector<float>> out;
	ofParameter<std::vector<int>> cycleCountOut;
	ofParameter<std::vector<int>> trigger;
	
	ofEventListeners listeners;
	
	// ───────────── Internal State ─────────────
	float basePPQ = 0.0f;
	float baseBeats = 0.0f;
	float lastPPQ = 0.0f;
	float lastBeats = 0.0f;
	
	std::vector<int> lastCycleCount;
	std::vector<float> accumulatedPhase;
	std::vector<int> jumpCounters; // Tracks 3-frame duration
	
	static constexpr float FRAME_RATE = 60.0f;
	
	// ───────────── Utilities ─────────────
	static inline float frac(float x){
		return x - std::floor(x);
	}
	
	static inline float phaseErrorShortest(float target, float current){
		float diff = frac(target) - frac(current);
		if(diff > 0.5f) diff -= 1.0f;
		if(diff < -0.5f) diff += 1.0f;
		return diff;
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
	
	float getBeats() const {
		const auto& v = beatTransport.get();
		return v.empty() ? 0.0f : v[0];
	}
	
	void resizeState(size_t n){
		if(n == 0) n = 1;
		
		if(lastCycleCount.size() != n){
			lastCycleCount.assign(n, 0);
			accumulatedPhase.assign(n, 0.0f);
			jumpCounters.assign(n, 0);
			cycleCountOut = lastCycleCount;
			
			auto o = out.get();
			o.assign(n, 0.0f);
			out = o;
			
			auto t = trigger.get();
			t.assign(n, 0);
			trigger = t;
		}
	}
	
	// ───────────── Core Logic ─────────────
	void compute(){
		const float beatsTrans = std::max(0.0f, getBeats());
		const float ppqVal = std::max(0.0f, getPPQ());
		
		float beatsAbs, deltaBeats;
		
		// Calculate global transport delta
		if(beatsTrans > 0.0f || ppqVal == 0.0f){
			beatsAbs = beatsTrans - baseBeats;
			deltaBeats = beatsTrans - lastBeats;
			lastBeats = beatsTrans;
		} else {
			beatsAbs = (ppqVal - basePPQ) / 24.0f;
			deltaBeats = (ppqVal - lastPPQ) / 24.0f;
			lastPPQ = ppqVal;
		}

		const auto& beatsV = beats.get();
		const auto& phV = initPhase.get();
		const auto& fmV = fmMod.get();
		const auto& jumpV = jump.get();

		const size_t n = std::max({beatsV.size(), phV.size(), fmV.size(), jumpV.size(), (size_t)1});
		resizeState(n);

		std::vector<float> outV(n, 0.0f);
		std::vector<int> cycV(n, 0);
		std::vector<int> trigV(n, 0);

		const int loopOn = loop.get();
		const int fixed = fixednum.get();
		const int forward = forwardPLL.get();
		const float rTime = std::max(0.01f, resyncTime.get());

		for(size_t i = 0; i < n; ++i){
			// 1. Inputs
			float beatDur = getIdx(beatsV, i, 1.0f);
			if(beatDur <= 0.0f) beatDur = 0.0001f;
			const float fm = getIdx(fmV, i, 1.0f);
			const float phOff = getIdx(phV, i, 0.0f);
			const int jumpIn = getIdx(jumpV, i, 0);

			// 2. Base Freq & Ideal Phase
			const float baseFreq = 1.0f / beatDur;
			const float idealCycles = beatsAbs * baseFreq;
			const float idealPhase = idealCycles + phOff;
			
			// 3. Jump Logic (3 Frames Hold)
			if(jumpIn > 0){
				jumpCounters[i] = 3;
			}
			
			bool isJumping = (jumpCounters[i] > 0);

			// 4. Phase Integration
			if(isJumping){
				// Hard Sync
				accumulatedPhase[i] = idealPhase;
				jumpCounters[i]--;
			} else {
				// Normal Operation: FM + PLL
				
				// FM Component
				const float fmPhaseIncrement = deltaBeats * baseFreq * fm;
				accumulatedPhase[i] += fmPhaseIncrement;
				
				// PLL Component
				const float fmDeviation = std::abs(fm - 1.0f);
				const float pllActive = std::exp(-fmDeviation * 5.0f);
				
				// Correction Rate = Error * Weight / Time
				float correction = 0.0f;
				float shortestError = phaseErrorShortest(idealPhase, accumulatedPhase[i]);
				float rawCorrection = (shortestError * pllActive) / (rTime * FRAME_RATE);
				
				if(forward > 0){
					if(shortestError > 0.0f){
						// Behind: accelerate
						correction = rawCorrection;
					} else {
						// Ahead: slow down (don't reverse)
						correction = std::max(rawCorrection, -fmPhaseIncrement);
					}
					
					// Snap to sync if extremely close
					if(pllActive > 0.99f && std::abs(shortestError) < 0.001f){
						accumulatedPhase[i] = idealPhase;
						correction = 0.0f;
					}
				} else {
					// Bidirectional
					correction = rawCorrection;
				}
				
				accumulatedPhase[i] += correction;
			}

			// 5. Wrap & Output
			const int prevCyc = lastCycleCount[i];
			const int cyc = std::max(0, (int)std::floor(accumulatedPhase[i]));
			
			// Trigger on cycle change
			const bool wrapped = (cyc > prevCyc);
			
			float ph = frac(accumulatedPhase[i]);

			// Fixed Count Logic
			if(fixed > 0 && cyc >= fixed){
				ph = 0.0f;
				accumulatedPhase[i] = fixed;
			}

			// One-shot Logic
			if(loopOn == 0 && cyc >= 1){
				ph = 0.0f;
				accumulatedPhase[i] = 1.0f;
			}

			outV[i] = ph;
			cycV[i] = cyc;
			trigV[i] = wrapped ? 1 : 0;
		}

		out = outV;
		cycleCountOut = cycV;
		trigger = trigV;
		lastCycleCount = cycV;
	}
};
