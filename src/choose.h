#ifndef choose_h
#define choose_h

#include "ofxOceanodeNodeModel.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <set>
#include <cfloat>
#include <cstdint>

class choose : public ofxOceanodeNodeModel {
public:
	choose()
	: ofxOceanodeNodeModel("Choose")
	{
		// ---------- Detailed description ----------
		// Shown in the node’s inspector (like other Oceanode nodes).
		description =
R"(Choose — deterministic weighted/urn selection

Outputs values from the Input list when Trigger rises, supporting:
• Weighted random selection
• URN mode (draw without replacement, auto-refill when depleted)
• Unique group selection (all rising triggers pick different items)
• Deterministic seeding (Seed ≠ 0) or non-deterministic (Seed = 0)

PARAMETERS
- Input (vector<float>): 
	The catalog of values to choose from. Output values are taken from this list.

- Weights (vector<float>):
	Selection weights matching Input (if size=1, the single value is replicated; 
	if shorter than Input, only the provided prefix is used and the rest are treated as 0).
	Internally normalized. If the sum is 0, falls back to uniform selection.

- Trigger (vector<int>):
	Rising-edge detector per position. Wherever lastTrigger==0 and newTrigger==1, 
	a choice is generated for that position. The output vector mirrors Trigger’s size.

- URN Seq (bool):
	When ON, items are drawn without replacement from a shuffled permutation of Input.
	The permutation refills automatically when exhausted (new shuffle with same seeding rules).

- Unique (bool):
	When ON, all positions that have a rising edge in the *same* evaluation receive
	mutually distinct items. In URN mode, uniqueness is guaranteed by the urn itself.
	In Weighted mode, items are picked without replacement for that batch.

- Seed (int):
	0   → Non-deterministic (std::random_device used; different runs differ).
	≠ 0 → Deterministic. For the same Seed, Input contents/size, Trigger pattern and Weights,
		  the sequence of choices is reproducible.
	Internals:
	  • genEvent uses Seed to drive weighted selections and unique batches.
	  • genUrn uses a stable mix of (Seed, Input.size()) to shuffle URN permutations.

OUTPUT
- Output (vector<float>):
	Contains the chosen values for each triggered position (same size as Trigger).
	Non-triggered positions preserve their previous output values.

NOTES
- Changing Seed reseeds the generators and rebuilds the URN permutation.
- Changing Input or toggling URN rebuilds the URN permutation (maintaining determinism if Seed≠0).
- Immediate repeat prevention in URN mode per position (if possible).
)";

		// ---------- parameters ----------
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(weights.set("Weights", {1.0f}, {0.0f}, {1.0f}));
		addParameter(trigger.set("Trigger", {0}, {0}, {1}));
		addParameter(urn.set("URN Seq", false));
		addParameter(unique.set("Unique", false));
		addParameter(seedParam.set("Seed", 0, INT_MIN, INT_MAX)); // deterministic control

		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

		// ---------- listeners ----------
		listeners.push(trigger.newListener([this](std::vector<int> &t){
			chooseValues(t);
		}));

		listeners.push(input.newListener([this](std::vector<float> &){
			resetUrn(); // input content/size affects urn permutation
		}));

		listeners.push(urn.newListener([this](bool &){
			resetUrn(); // urn toggled → rebuild urn permutation
		}));

		listeners.push(seedParam.newListener([this](int &){
			reseedAll(); // reseed both RNG streams
			resetUrn();  // urn permutation depends on seed
		}));

		// ---------- initial state ----------
		reseedAll();
		resetUrn();
		lastTrigger = trigger.get();
	}

private:
	// -------- params --------
	ofParameter<std::vector<float>> input;
	ofParameter<std::vector<float>> weights;
	ofParameter<std::vector<int>>   trigger;
	ofParameter<bool>               urn;
	ofParameter<bool>               unique;
	ofParameter<int>                seedParam;

	ofParameter<std::vector<float>> output;

	// -------- state --------
	std::vector<int>  lastTrigger;
	std::vector<int>  urnSequence;
	size_t            urnIndex = 0;
	std::vector<int>  lastChosenIndices;

	// Separate RNGs for events & urn shuffles
	std::mt19937                       genEvent;
	std::mt19937                       genUrn;
	std::uniform_real_distribution<>   dist{0.0, 1.0};

	ofEventListeners listeners;

	// ---- tiny deterministic mixers ----
	static inline std::uint32_t mix32(std::uint32_t x){
		x += 0x9e3779b9u;
		x ^= x >> 16;
		x *= 0x85ebca6bu;
		x ^= x >> 13;
		x *= 0xc2b2ae35u;
		x ^= x >> 16;
		return x;
	}
	static inline std::uint32_t mixPair(std::uint32_t a, std::uint32_t b){
		return mix32(a ^ (mix32(b) + 0x9e3779b9u + (a<<6) + (a>>2)));
	}

	void reseedAll(){
		int s = seedParam.get();
		if (s == 0){
			// non-deterministic
			std::random_device rd;
			genEvent.seed((std::uint32_t)mixPair(rd(), (std::uint32_t)rd()));
			genUrn  .seed((std::uint32_t)mixPair(rd(), (std::uint32_t)rd()));
		} else {
			// deterministic
			std::uint32_t base = (std::uint32_t)s;
			genEvent.seed(mixPair(base, 0xA5A5A5A5u));
			// genUrn is finalized in resetUrn() because it depends on Input.size()
		}
	}

	void chooseValues(const std::vector<int>& newTrigger) {
		if (input->empty()) return;

		std::vector<float> currentOutput = output.get();
		size_t size = newTrigger.size();
		currentOutput.resize(size, 0.0f);
		lastTrigger.resize(size, 0);
		lastChosenIndices.resize(size, -1);

		if (unique) {
			// group rising edges together to enforce uniqueness
			std::vector<size_t> rising;
			rising.reserve(size);
			for (size_t i = 0; i < size; ++i) {
				if (lastTrigger[i] == 0 && newTrigger[i] == 1) rising.push_back(i);
			}

			if (!rising.empty()) {
				auto vals = chooseUniqueValues(rising.size());
				for (size_t i = 0; i < rising.size(); ++i) {
					currentOutput[rising[i]] = vals[i];
				}
			}
		} else {
			// independent weighted/urn picks per rising edge
			for (size_t i = 0; i < size; ++i) {
				if (lastTrigger[i] == 0 && newTrigger[i] == 1) {
					currentOutput[i] = chooseValue(i);
				}
			}
		}

		output = currentOutput;
		lastTrigger = newTrigger;
	}

	// Select multiple unique values (respecting URN/weights)
	std::vector<float> chooseUniqueValues(size_t count) {
		std::vector<float> result;
		count = std::min(count, input->size());

		if (urn) {
			// take next 'count' items from urn
			if (urnIndex + count > urnSequence.size()) resetUrn();
			for (size_t i = 0; i < count; ++i) {
				result.push_back(input->at(urnSequence[urnIndex++]));
			}
		} else {
			// Weighted unique: remove chosen indices as we go
			std::vector<size_t> available(input->size());
			std::iota(available.begin(), available.end(), 0);

			std::vector<float> currentWeights;
			if (weights->size() <= 1) {
				currentWeights.assign(input->size(), 1.0f);
			} else {
				currentWeights.assign(weights->begin(),
									  weights->begin() + std::min(weights->size(), input->size()));
			}

			for (size_t k = 0; k < count; ++k) {
				float sum = std::accumulate(currentWeights.begin(), currentWeights.end(), 0.0f);
				if (sum <= 0.0f) {
					// fallback to uniform among remaining
					std::fill(currentWeights.begin(), currentWeights.end(), 1.0f);
					sum = (float)currentWeights.size();
				}

				float r = (float)dist(genEvent) * sum;
				float acc = 0.0f;
				size_t pickIdx = currentWeights.size() - 1;

				for (size_t j = 0; j < currentWeights.size(); ++j) {
					acc += currentWeights[j];
					if (r <= acc) { pickIdx = j; break; }
				}

				size_t chosen = available[pickIdx];
				result.push_back(input->at(chosen));

				// remove chosen from pools
				available.erase(available.begin() + (long)pickIdx);
				currentWeights.erase(currentWeights.begin() + (long)pickIdx);
			}
		}
		return result;
	}

	float chooseValue(size_t index) {
		if (urn) {
			if (urnIndex >= urnSequence.size()) {
				resetUrn();
			}
			int chosenIndex = urnSequence[urnIndex++];

			// avoid immediate repeat on the same lane if possible
			if (chosenIndex == lastChosenIndices[index] && input->size() > 1) {
				urnIndex %= urnSequence.size();
				chosenIndex = urnSequence[urnIndex++];
			}
			lastChosenIndices[index] = chosenIndex;
			return input->at(chosenIndex);
		} else {
			// normalize weights
			std::vector<float> normW;
			if (weights->empty() || weights->size() == 1) {
				normW.assign(input->size(), 1.0f / std::max<size_t>(1, input->size()));
			} else {
				float sum = std::accumulate(weights->begin(), weights->end(), 0.0f);
				if (sum <= 0.0f) {
					normW.assign(input->size(), 1.0f / std::max<size_t>(1, input->size()));
				} else {
					normW.resize(std::min(weights->size(), input->size()));
					for (size_t i = 0; i < normW.size(); ++i) normW[i] = weights->at(i) / sum;
				}
			}

			float r = (float)dist(genEvent);
			float acc = 0.0f;
			for (size_t i = 0; i < normW.size(); ++i) {
				acc += normW[i];
				if (r <= acc) return input->at(i);
			}
			return input->back(); // numeric drift guard
		}
	}

	void resetUrn() {
		urnSequence.clear();
		urnSequence.reserve(input->size());
		for (size_t i = 0; i < input->size(); ++i) urnSequence.push_back((int)i);

		// Deterministic urn permutation if Seed ≠ 0; otherwise random_device
		int s = seedParam.get();
		if (s == 0) {
			std::random_device rd;
			genUrn.seed((std::uint32_t)mixPair(rd(), (std::uint32_t)input->size()));
		} else {
			// Stable for the same (Seed, Input.size())
			genUrn.seed((std::uint32_t)mixPair((std::uint32_t)s, (std::uint32_t)input->size()));
		}
		std::shuffle(urnSequence.begin(), urnSequence.end(), genUrn);
		urnIndex = 0;
	}
};

#endif /* choose_h */
