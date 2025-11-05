#ifndef chancePass_h
#define chancePass_h

#include "ofxOceanodeNodeModel.h"

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
				tempOut[i] = (dist(mt[i]) < getProbability(i)) ? vf[i] : output->at(i);
			}
			output = tempOut;
		}));

		// GATE STREAM
		listeners.push(gateIn.newListener([this](std::vector<float> &vf){
			ensureVectorSize(vf.size());
			std::vector<float> tempOut(output->size());
			for (size_t i = 0; i < vf.size(); ++i) {
				if (vf[i] > 0.0f && dist(mt[i]) < getProbability(i)) {
					tempOut[i] = vf[i];
				} else {
					tempOut[i] = 0.0f;
				}
			}
			output = tempOut;
		}));

		// SEED CHANGE
		listeners.push(seed.newListener([this](std::vector<int> &){
			setSeed();
		}));
	}

private:
	ofEventListeners listeners;

	ofParameter<std::vector<float>> numberIn;
	ofParameter<std::vector<float>> gateIn;
	ofParameter<std::vector<int>>   seed;
	ofParameter<std::vector<float>> probability;
	ofParameter<std::vector<float>> output;

	std::vector<std::mt19937>       mt;
	std::uniform_real_distribution<float> dist;

	void ensureVectorSize(size_t newSize) {
		if (output->size() != newSize) {
			// resize output
			output.set(std::vector<float>(newSize, 0.0f));
			// resize RNGs
			mt.resize(newSize);
			// reseed to match size
			setSeed();
		}
	}

	float getProbability(size_t index) {
		const auto &p = probability.get();
		if (p.size() == output->size())
			return p[index];
		return p[0];
	}

	void setSeed() {
		const auto &s = seed.get();
		for (size_t i = 0; i < mt.size(); ++i) {
			if (s.size() == mt.size()) {
				// per-lane seed
				mt[i].seed(s[i]);
			} else {
				if (s[0] == 0) {
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
