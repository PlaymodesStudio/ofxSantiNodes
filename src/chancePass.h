#ifndef chancePass_h
#define chancePass_h

#include "ofxOceanodeNodeModel.h"
#include <cstdint>
#include <cfloat>
#include <random>
#include <vector>
#include <algorithm>

class chancePass : public ofxOceanodeNodeModel {
public:
	chancePass() : ofxOceanodeNodeModel("Chance Pass"){}

	void setup(){
		description =
R"(Chance Pass — Bernoulli pass-through (rising-edge, deterministic)

• Gate: each 0→1 transition performs ONE probability test.
• Prob: pass probability in [0..1] (scalar or per-lane).
• Seed: 0 = non-deterministic; ≠0 = deterministic (repeats across runs).
Seed is applied ONLY when it actually changes; no hidden reseeding per tick.)";

		addParameter(numberIn.set("Number", {0.0f}, {FLT_MIN}, {FLT_MAX}));
		addParameter(gateIn.set("Gate",   {0.0f},   {0.0f},   {1.0f}));
		addParameter(probability.set("Prob", {0.5f}, {0.0f}, {1.0f}));
		addParameter(seed.set("Seed", {0}, {(INT_MIN+1)/2}, {(INT_MAX-1)/2}));
		addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {FLT_MAX}));

		lastGate.assign(1, 0.0f);
		gateEvt.assign(1, 0);
		numEvt.assign(1, 0);

		// initial seed application (and counters reset once)
		lastSeedApplied_.clear();
		applySeedIfChanged(/*force=*/true);

		listeners.push(numberIn.newListener([this](std::vector<float> &vf){
			ensureSize(vf.size());
			std::vector<float> tempOut(output->size());
			for(size_t i=0;i<vf.size();++i){
				const float p = probAt(i);
				const double u  = rngNum(i, ++numEvt[i]);     // per-lane
				const double u2 = rngNum(i, ++globalEvtNum);  // global
				const double ur = mix01(u, u2);
				tempOut[i] = (float)((ur < p) ? vf[i] : output->at(i));
			}
			output = tempOut;
		}));

		listeners.push(gateIn.newListener([this](std::vector<float> &vf){
			ensureSize(vf.size());
			std::vector<float> tempOut(output->size());
			for(size_t i=0;i<vf.size();++i){
				const bool rising = (lastGate[i] <= 0.0f) && (vf[i] > 0.0f);
				lastGate[i] = vf[i];

				if(rising){
					const float p = probAt(i);
					const double u  = rngGate(i, ++gateEvt[i]);     // per-lane
					const double u2 = rngGate(i, ++globalEvtGate);  // global
					const double ur = mix01(u, u2);
					tempOut[i] = (float)((ur < p) ? vf[i] : 0.0f);
				}else{
					tempOut[i] = (vf[i] > 0.0f) ? output->at(i) : 0.0f;
				}
			}
			output = tempOut;
		}));

		// Only apply seed when it actually CHANGES
		listeners.push(seed.newListener([this](std::vector<int> &){
			applySeedIfChanged(/*force=*/false);
		}));
	}

private:
	// ---------- parameters ----------
	ofParameter<std::vector<float>> numberIn;
	ofParameter<std::vector<float>> gateIn;
	ofParameter<std::vector<int>>   seed;
	ofParameter<std::vector<float>> probability;
	ofParameter<std::vector<float>> output;

	ofEventListeners listeners;

	// ---------- state ----------
	std::vector<float>    lastGate;
	std::vector<uint64_t> gateEvt;
	std::vector<uint64_t> numEvt;

	uint64_t globalEvtGate = 0;
	uint64_t globalEvtNum  = 0;

	uint64_t baseSeedGate  = 0;
	uint64_t baseSeedNum   = 0;

	// cache last applied seed vector to prevent no-op reseeds
	std::vector<int> lastSeedApplied_;

	// ---------- SplitMix64 tools ----------
	static inline uint64_t splitmix64(uint64_t x){
		x += 0x9e3779b97f4a7c15ULL;
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
		x = x ^ (x >> 31);
		return x;
	}
	static inline uint64_t mix3(uint64_t a, uint64_t b, uint64_t c){
		return splitmix64(a ^ splitmix64(b + 0x632BE59BD9B4E019ULL) ^ (c * 0x9E3779B97F4A7C15ULL));
	}
	static inline double u64_to_unit(uint64_t u){
		return (u >> 11) * (1.0/9007199254740992.0); // 2^53
	}
	static inline double mix01(double u, double v){
		const uint64_t A = (uint64_t)(u * 9007199254740992.0);
		const uint64_t B = (uint64_t)(v * 9007199254740992.0);
		return (double)(A ^ B) / 9007199254740992.0;
	}

	inline double rngGate(size_t lane, uint64_t evt) const {
		return u64_to_unit(mix3(baseSeedGate, (uint64_t)lane, evt));
	}
	inline double rngNum(size_t lane, uint64_t evt) const {
		return u64_to_unit(mix3(baseSeedNum,  (uint64_t)lane, evt));
	}

	inline float probAt(size_t i) const {
		const auto &p = probability.get();
		if(p.empty()) return 0.0f;
		if(p.size() == output->size()) return std::clamp(p[i], 0.0f, 1.0f);
		return std::clamp(p.front(), 0.0f, 1.0f);
	}

	void ensureSize(size_t n){
		if(output->size() == n) return;
		output.set(std::vector<float>(n, 0.0f));

		// expand preserving counters; shrink keeps existing lanes' counters
		if(n > lastGate.size()){
			lastGate.resize(n, 0.0f);
			gateEvt.resize(n, 0);
			numEvt.resize(n, 0);
		}else{
			lastGate.resize(n);
			gateEvt.resize(n);
			numEvt.resize(n);
		}
		// NOTE: no seed re-applied here
	}

	void applySeedIfChanged(bool force){
		const auto &cur = seed.get();
		if(!force && cur == lastSeedApplied_) return; // no-op update, ignore
		lastSeedApplied_ = cur;

		if(cur.empty() || cur[0] == 0){
			// non-deterministic once
			std::random_device rd;
			uint64_t r0 = ((uint64_t)rd() << 32) ^ rd();
			uint64_t r1 = ((uint64_t)rd() << 32) ^ rd();
			baseSeedGate = splitmix64(r0 ^ 0xA5A5A5A5ULL);
			baseSeedNum  = splitmix64(r1 ^ 0x5A5A5A5AULL);
		} else if (cur.size() == output->size()){
			// fold per-lane seeds to two bases
			uint64_t accG = 0x123456789ABCDEF0ULL;
			uint64_t accN = 0x0FEDCBA987654321ULL;
			for(size_t i=0;i<cur.size();++i){
				uint64_t s = (uint64_t)(int64_t)cur[i];
				accG = mix3(accG, s, i+1);
				accN = mix3(accN, s ^ 0xDEADBEEFULL, (i+1)*7);
			}
			baseSeedGate = accG;
			baseSeedNum  = accN;
		} else {
			// single deterministic seed
			uint64_t s = (uint64_t)(int64_t)cur[0];
			baseSeedGate = mix3(s, 0xA5A5A5A5A5A5A5A5ULL, 0x1);
			baseSeedNum  = mix3(s ^ 0xDEADBEEFCAFEBABEULL, 0x5A5A5A5A5A5A5A5AULL, 0x2);
		}

		// reset counters ONLY when seed actually changes
		std::fill(gateEvt.begin(), gateEvt.end(), 0);
		std::fill(numEvt.begin(),  numEvt.end(),  0);
		globalEvtGate = 0;
		globalEvtNum  = 0;
	}
};

#endif /* chancePass_h */
