#ifndef probSeq_h
#define probSeq_h

#include "ofxOceanodeNodeModel.h"
#include <random>
#include <climits>    // INT_MAX
#include <algorithm>  // std::clamp

class probSeq : public ofxOceanodeNodeModel {
public:
	probSeq()
	: ofxOceanodeNodeModel("Probabilistic Step Sequencer")
	, gen(rd())
	, dist(0.0, 1.0) {}

	~probSeq() {}

	void setup() override {
		// Parameters
		addParameter(index.set("Index", 0, 0, 1));
		addParameter(stepsVec.set("Steps[]", {0.0f}, {0.0f}, {1.0f}));
		addParameter(seed.set("Seed", 0, 0, INT_MAX));
		addOutputParameter(output.set("Output", 0, 0, 1));

		// Recompute output whenever index changes (pulse lasts exactly the step duration)
		indexListener = index.newListener([this](int &) { updateOutput(); });

		// Reseed RNG when seed changes
		seedListener = seed.newListener([this](int &) { resetGenerator(); });

		// Keep index within bounds when step vector changes; recompute output
		stepsVecListener = stepsVec.newListener([this](std::vector<float> &steps) {
			const int maxIdx = std::max(0, static_cast<int>(steps.size()) - 1);
			index.setMax(maxIdx);
			index = std::clamp(index.get(), 0, maxIdx);
			updateOutput();
		});

		// Initialize RNG policy and state
		resetGenerator();

		// Initialize index bounds from initial steps
		const int maxIdx = std::max(0, static_cast<int>(stepsVec.get().size()) - 1);
		index.setMax(maxIdx);
		index = std::clamp(index.get(), 0, maxIdx);

		lastIndex = -1;
		updateOutput(); // produce initial output
	}

private:
	// Parameters
	ofParameter<int> index;
	ofParameter<std::vector<float>> stepsVec;
	ofParameter<int> seed;
	ofParameter<int> output;

	// Listeners
	ofEventListener indexListener;
	ofEventListener stepsVecListener;
	ofEventListener seedListener;

	// State
	int lastIndex = -1;

	// RNG
	std::random_device rd;
	std::mt19937 gen;
	std::uniform_real_distribution<double> dist;

	void updateOutput() {
		const auto &steps = stepsVec.get();
		if (steps.empty()) {
			output.set(0);
			lastIndex = -1;
			return;
		}

		const int currentIndex = index.get();
		if (currentIndex == lastIndex) {
			// No step change → hold current output value for the remainder of the step
			return;
		}
		lastIndex = currentIndex;

		const int stepsSize = static_cast<int>(steps.size());
		const int modIndex = (stepsSize > 0) ? (currentIndex % stepsSize) : 0;

		// Clamp probability to [0,1] to be safe
		const float p = std::clamp(steps[modIndex], 0.0f, 1.0f);

		// Decide gate for THIS step and set it immediately
		const bool gate = (dist(gen) < static_cast<double>(p));
		output.set(gate ? 1 : 0);
	}

	void resetGenerator() {
		if (seed.get() != 0) {
			gen.seed(static_cast<uint32_t>(seed.get()));
		} else {
			gen.seed(rd()); // non-deterministic seed
		}
	}
};

#endif /* probSeq_h */
