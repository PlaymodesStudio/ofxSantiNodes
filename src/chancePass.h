#ifndef chancePass_h
#define chancePass_h

#include "ofxOceanodeNodeModel.h"
// Transport mode needs ofxOceanode's global transport (feature-globalTransport branch).
// Without it the node compiles exactly as before.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
#include "ofxOceanodeDeterministicRandom.h"
#endif

#include <vector>
#include <random>
#include <cfloat>   // for FLT_MIN, FLT_MAX
#include <climits>  // for INT_MIN, INT_MAX

class chancePass : public ofxOceanodeNodeModel {
public:
	chancePass() : ofxOceanodeNodeModel("Chance Pass") {}

	void setup() {
		addParameter(numberIn.set("Number", {0.0f}, {FLT_MIN}, {FLT_MAX}));
		addParameter(gateIn.set("Gate",   {0.0f},   {0.0f},    {1.0f}));
		addParameter(seed.set("Seed",     {0},      {(INT_MIN+1)/2}, {(INT_MAX-1)/2}));
		addParameter(probability.set("Prob", {0.5f}, {0.0f}, {1.0f}));
		addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {FLT_MAX}));

		dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
		mt.resize(1);
		setSeed();

		// NUMBER STREAM
		listeners.push(numberIn.newListener([this](std::vector<float> &vf){
			ensureVectorSize(vf.size());
			std::vector<float> tempOut(output->size());
			for (size_t i = 0; i < vf.size(); ++i) {
				// if pass → take new number, else keep old output
				tempOut[i] = roll(i) ? vf[i] : output->at(i);
			}
			lastStreamWasGate = false;
			output = tempOut;
		}));

		// GATE STREAM
		listeners.push(gateIn.newListener([this](std::vector<float> &vf){
			ensureVectorSize(vf.size());
			std::vector<float> tempOut(output->size());
			for (size_t i = 0; i < vf.size(); ++i) {
				if (vf[i] > 0.0f && roll(i)) {
					tempOut[i] = vf[i];
				} else {
					tempOut[i] = 0.0f;
				}
			}
			lastStreamWasGate = true;
			output = tempOut;
		}));

		// SEED CHANGE
		listeners.push(seed.newListener([this](std::vector<int> &){
			setSeed();
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
			if (syncToTransport) reapplyTransport();
#endif
		}));

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
		// ---- Sync To Transport ----
		// The pass/fail roll becomes a pure function of (Seed, Step, lane): one decision per
		// lane per step, so scrubbing gives the same pattern as playback. Gate mode is fully
		// position-exact; Number mode holds the last passed value, which after a jump is
		// whatever passed since the jump.
		sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
		addInspectorParameter(syncToTransport.set("Sync To Transport", false));
		listeners.push(syncToTransport.newListener([this](bool &b){
			setStepInputVisible(b);
			if (b) reapplyTransport();
		}));
		listeners.push(stepIn.newListener([this](std::vector<float> &){
			if (syncToTransport) reapplyTransport();
		}));
#endif
	}

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	void loadBeforeConnections(ofJson &json) override {
		// Restore the mode before connections so a saved "Step" connection finds its input.
		deserializeParameter(json, syncToTransport);
	}
#endif

private:
	ofEventListeners listeners;

	// ---- transport mode ----
	bool lastStreamWasGate = true;

	bool roll(size_t i) {
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
		if (syncToTransport) return transportRoll(i);
#endif
		return dist(mt[i]) < getProbability(i);
	}

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	ofParameter<bool>               syncToTransport;
	ofParameter<std::vector<float>> stepIn; // only present in Sync To Transport mode
	uint64_t sessionSalt = 0;

	bool transportRoll(size_t i) {
		const auto &steps = stepIn.get();
		const float stepValue = steps.empty() ? 0.0f : (i < steps.size() ? steps[i] : steps[0]);
		const int64_t step = ofxOceanodeDeterministicRandom::stepFromFloat(stepValue);
		return ofxOceanodeDeterministicRandom::uniform(laneKey(i), step, 0) < getProbability(i);
	}

	// Same seed rules as setSeed(): per-lane seeds, or single seed + lane offset; 0 = session-random.
	uint64_t laneKey(size_t i) {
		const auto &s = seed.get();
		int laneSeed = 0;
		if (!s.empty()) laneSeed = (s.size() == output->size()) ? s[i] : (s[0] == 0 ? 0 : s[0] + (int)i);
		if (laneSeed == 0) return ofxOceanodeDeterministicRandom::mix(sessionSalt ^ (uint64_t)i);
		return ofxOceanodeDeterministicRandom::seedKey(laneSeed, sessionSalt);
	}

	// Re-evaluate the current input for the current step (step or seed changed).
	void reapplyTransport() {
		if (lastStreamWasGate) {
			auto g = gateIn.get();
			gateIn.set(g);
		} else {
			auto n = numberIn.get();
			numberIn.set(n);
		}
	}

	void setStepInputVisible(bool visible) {
		const bool present = getParameterGroup().contains("Step");
		if (visible && !present) {
			addParameter(stepIn.set("Step", {0.0f}, {0.0f}, {FLT_MAX}));
		} else if (!visible && present) {
			getOceanodeParameter(stepIn).removeAllConnections();
			removeParameter("Step");
		}
	}
#endif

	ofParameter<std::vector<float>> numberIn;
	ofParameter<std::vector<float>> gateIn;
	ofParameter<std::vector<int>>   seed;
	ofParameter<std::vector<float>> probability;
	ofParameter<std::vector<float>> output;

	std::vector<std::mt19937>       mt;
	std::uniform_real_distribution<float> dist;

	void ensureVectorSize(size_t newSize) {
		const bool outputSizeChanged = output->size() != newSize;
		const bool rngSizeChanged = mt.size() != newSize;
		if (outputSizeChanged) {
			output.set(std::vector<float>(newSize, 0.0f));
		}
		if (rngSizeChanged) {
			mt.resize(newSize);
		}
		if (outputSizeChanged || rngSizeChanged) {
			setSeed();
		}
	}

	float getProbability(size_t index) {
		const auto &p = probability.get();
		if (p.empty()) return 0.5f;
		if (p.size() == output->size())
			return p[index];
		return p[0];
	}

	void setSeed() {
		const auto &s = seed.get();
		for (size_t i = 0; i < mt.size(); ++i) {
			if (s.size() == mt.size() && !s.empty()) {
				// per-lane seed
				mt[i].seed(s[i]);
			} else {
				if (s.empty() || s[0] == 0) {
					// nondeterministic
					std::random_device rd;
					mt[i].seed(rd());
				} else {
					// single seed + lane offset
					mt[i].seed(s[0] + (int)i);
				}
			}
		}
	}
};

#endif /* chancePass_h */
