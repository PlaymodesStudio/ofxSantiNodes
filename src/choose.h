#ifndef choose_h
#define choose_h

#include "ofxOceanodeNodeModel.h"
// Transport mode needs ofxOceanode's global transport (feature-globalTransport branch).
// Without it the node compiles exactly as before.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
#include "ofxOceanodeDeterministicRandom.h"
#endif
#include <map>
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
			if (isTransportMode()) return; // Step drives the node in transport mode
			chooseValues(t);
		}));

		listeners.push(input.newListener([this](std::vector<float> &){
			resetUrn(); // input content/size affects urn permutation
			if (isTransportMode()) computeTransport();
		}));

		listeners.push(urn.newListener([this](bool &){
			resetUrn(); // urn toggled → rebuild urn permutation
			if (isTransportMode()) computeTransport();
		}));

		listeners.push(seedParam.newListener([this](int &){
			reseedAll(); // reseed both RNG streams
			resetUrn();  // urn permutation depends on seed
			if (isTransportMode()) computeTransport();
		}));

		listeners.push(weights.newListener([this](std::vector<float> &){
			if (isTransportMode()) computeTransport();
		}));

		listeners.push(unique.newListener([this](bool &){
			if (isTransportMode()) computeTransport();
		}));

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
		// ---------- Sync To Transport ----------
		// Output is a pure function of (Input, Weights, URN, Unique, Seed, Step): every lane
		// picks at every step, so scrubbing the timeline gives exactly what playback gives.
		// Trigger is ignored in this mode; connect Step to a Phasor "Cycle" output.
		sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
		addInspectorParameter(syncToTransport.set("Sync To Transport", false));
		listeners.push(syncToTransport.newListener([this](bool &b){
			setStepInputVisible(b);
			if (b) computeTransport();
		}));
		listeners.push(stepIn.newListener([this](std::vector<float> &){
			if (syncToTransport) computeTransport();
		}));
#endif

		// ---------- initial state ----------
		reseedAll();
		resetUrn();
		lastTrigger = trigger.get();
	}

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	void loadBeforeConnections(ofJson &json) override {
		// Restore the mode before connections so a saved "Step" connection finds its input.
		deserializeParameter(json, syncToTransport);
	}
#endif

private:
	// ======================= transport mode =======================
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	bool isTransportMode() const { return syncToTransport.get(); }
	ofParameter<bool>               syncToTransport;
	ofParameter<std::vector<float>> stepIn; // only present in Sync To Transport mode
	uint64_t sessionSalt = 0;

	void setStepInputVisible(bool visible){
		const bool present = getParameterGroup().contains("Step");
		if (visible && !present){
			addParameter(stepIn.set("Step", {0.0f}, {0.0f}, {FLT_MAX}));
		} else if (!visible && present){
			getOceanodeParameter(stepIn).removeAllConnections();
			removeParameter("Step");
		}
	}

	uint64_t transportKey(){
		return ofxOceanodeDeterministicRandom::seedKey(seedParam.get(), sessionSalt);
	}

	static int64_t floorDiv(int64_t a, int64_t b){
		int64_t q = a / b;
		if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
		return q;
	}

	// Permutation of [0, N) for a given bag, driven by the hash (Fisher-Yates).
	std::vector<int> bagPermutation(uint64_t key, int64_t bag, uint64_t stream, size_t N){
		std::vector<int> perm(N);
		std::iota(perm.begin(), perm.end(), 0);
		for (size_t j = N - 1; j > 0; --j){
			const uint64_t h = ofxOceanodeDeterministicRandom::hash(key, bag, (stream << 20) + j);
			std::swap(perm[j], perm[h % (j + 1)]);
		}
		return perm;
	}

	// Same weighting rules as chooseValue()'s non-URN branch.
	float weightedPick(float u){
		const size_t N = input->size();
		std::vector<float> normW;
		if (weights->empty() || weights->size() == 1){
			normW.assign(N, 1.0f / std::max<size_t>(1, N));
		} else {
			float sum = std::accumulate(weights->begin(), weights->end(), 0.0f);
			if (sum <= 0.0f){
				normW.assign(N, 1.0f / std::max<size_t>(1, N));
			} else {
				normW.resize(std::min(weights->size(), N));
				for (size_t i = 0; i < normW.size(); ++i) normW[i] = weights->at(i) / sum;
			}
		}
		float acc = 0.0f;
		for (size_t i = 0; i < normW.size(); ++i){
			acc += normW[i];
			if (u <= acc) return input->at(i);
		}
		return input->back();
	}

	// URN per lane: each bag of N steps visits every item once; like chooseValue(),
	// a bag never starts with the item that ended the previous one.
	float urnPick(uint64_t key, size_t lane, int64_t step){
		const size_t N = input->size();
		if (N == 1) return input->at(0);
		if (N == 2){ // no-repeat with two items is a strict alternation
			const int64_t start = (int64_t)(ofxOceanodeDeterministicRandom::hash(key, 0, lane) & 1);
			return input->at((size_t)(((step + start) % 2 + 2) % 2));
		}
		const int64_t bag = floorDiv(step, (int64_t)N);
		const size_t pos = (size_t)(step - bag * (int64_t)N);
		std::vector<int> perm = bagPermutation(key, bag, lane, N);
		if (perm[0] == bagPermutation(key, bag - 1, lane, N)[N - 1]) std::swap(perm[0], perm[1]);
		return input->at(perm[pos]);
	}

	// Lanes advancing on the same step get distinct items (Unique), like chooseUniqueValues().
	std::vector<float> uniquePicks(uint64_t key, int64_t step, size_t count){
		const size_t N = input->size();
		std::vector<float> result;
		result.reserve(count);
		if (urn){
			const size_t c = std::min(count, N);
			const int64_t stepsPerBag = std::max<int64_t>(1, (int64_t)(N / c));
			const int64_t bag = floorDiv(step, stepsPerBag);
			const size_t offset = (size_t)(step - bag * stepsPerBag) * c;
			const std::vector<int> perm = bagPermutation(key, bag, 0xC0FFEEull, N);
			for (size_t k = 0; k < count; ++k) result.push_back(input->at(perm[(offset + k) % N]));
			return result;
		}
		std::vector<size_t> available(N);
		std::iota(available.begin(), available.end(), 0);
		std::vector<float> currentWeights;
		if (weights->size() <= 1) currentWeights.assign(N, 1.0f);
		else currentWeights.assign(weights->begin(), weights->begin() + std::min(weights->size(), N));
		available.resize(currentWeights.size());
		for (size_t k = 0; k < count; ++k){
			const float u = ofxOceanodeDeterministicRandom::uniform(key, step, 0x100ull + k);
			if (available.empty()){ // more lanes than items: fall back to a plain weighted pick
				result.push_back(weightedPick(u));
				continue;
			}
			float sum = std::accumulate(currentWeights.begin(), currentWeights.end(), 0.0f);
			if (sum <= 0.0f){
				std::fill(currentWeights.begin(), currentWeights.end(), 1.0f);
				sum = (float)currentWeights.size();
			}
			const float r = u * sum;
			float acc = 0.0f;
			size_t pickIdx = currentWeights.size() - 1;
			for (size_t j = 0; j < currentWeights.size(); ++j){
				acc += currentWeights[j];
				if (r <= acc){ pickIdx = j; break; }
			}
			result.push_back(input->at(available[pickIdx]));
			available.erase(available.begin() + (long)pickIdx);
			currentWeights.erase(currentWeights.begin() + (long)pickIdx);
		}
		return result;
	}

	void computeTransport(){
		const auto &steps = stepIn.get();
		if (input->empty() || steps.empty()) return;
		const uint64_t key = transportKey();
		const size_t lanes = steps.size();
		std::vector<float> out(lanes, 0.0f);
		std::vector<int64_t> laneStep(lanes);
		for (size_t i = 0; i < lanes; ++i) laneStep[i] = ofxOceanodeDeterministicRandom::stepFromFloat(steps[i]);

		if (unique){
			std::map<int64_t, std::vector<size_t>> groups;
			for (size_t i = 0; i < lanes; ++i) groups[laneStep[i]].push_back(i);
			for (auto &group : groups){
				const auto vals = uniquePicks(key, group.first, group.second.size());
				for (size_t g = 0; g < group.second.size(); ++g) out[group.second[g]] = vals[g];
			}
		} else {
			for (size_t i = 0; i < lanes; ++i){
				out[i] = urn ? urnPick(key, i, laneStep[i])
				             : weightedPick(ofxOceanodeDeterministicRandom::uniform(key, laneStep[i], i));
			}
		}
		output = out;
	}
#else
	bool isTransportMode() const { return false; }
	void computeTransport(){}
#endif

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
