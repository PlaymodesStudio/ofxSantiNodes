// randomValues.h
#pragma once
#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <random>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cfloat>

class randomValues : public ofxOceanodeNodeModel {
public:
	randomValues() : ofxOceanodeNodeModel("Random Values") {}

	void setup() override {
		color = ofColor(0, 200, 255);
		description = "Generates a vector of random values when 'Generate' (void) is triggered.";

		// Parameters
		addParameter(generateVoid.set("Generate"));
		
		addParameter(size_Param.set("Size", 16, 1, 4096));              // single-value size
		addParameter(min_Param.set("Min", 0.0f, -FLT_MAX,  FLT_MAX));
		addParameter(max_Param.set("Max", 1.0f, -FLT_MAX,  FLT_MAX));
		addParameter(pow_Param.set("Pow", 0.0f, -1.0f, 1.0f));          // unipolar curvature
		addParameter(biPow_Param.set("BiPow", 0.0f, -1.0f, 1.0f));      // bipolar curvature
		addParameter(quant_Param.set("Quant", 0, 0, INT_MAX));          // 0 = no quantization
		addParameter(seed_Param.set("Seed", 0, INT_MIN, INT_MAX));

		addOutputParameter(output.set("Output", std::vector<float>{0.0f}, std::vector<float>{0.0f}, std::vector<float>{1.0f}));

		
		generateListener = generateVoid.newListener([this]{
			generateNow();
		});

		// keep output clamps in sync
		minListener = min_Param.newListener([this](float &v){
			output.setMin(std::vector<float>(1, v));
		});
		maxListener = max_Param.newListener([this](float &v){
			output.setMax(std::vector<float>(1, v));
		});

		// initial vector
		generateNow();
	}

private:
	// ---------- Parameters ----------
	ofParameter<int>    size_Param;
	ofParameter<float>  min_Param, max_Param;
	ofParameter<float>  pow_Param, biPow_Param;
	ofParameter<int>    quant_Param;
	ofParameter<int>    seed_Param;

	ofParameter<void>   generateVoid;

	// ---------- Output ----------
	ofParameter<std::vector<float>> output;

	// ---------- Listeners ----------
	ofEventListener generateListener;
	ofEventListener minListener, maxListener;

	// ---------- State ----------
	uint64_t unseededBump = 0; // ensure variety when Seed == 0

	// ----- small deterministic mixers -----
	static inline uint32_t mix32(uint32_t x){
		x += 0x9e3779b9u;
		x ^= x >> 16;
		x *= 0x85ebca6bu;
		x ^= x >> 13;
		x *= 0xc2b2ae35u;
		x ^= x >> 16;
		return x;
	}
	static inline uint32_t mixPair(uint32_t a, uint32_t b){
		return mix32(a ^ (mix32(b) + 0x9e3779b9u + (a<<6) + (a>>2)));
	}
	static inline float u32_to_unit(uint32_t u){
		// [0,1)
		constexpr double inv = 1.0 / 4294967296.0;
		return (float)(u * inv);
	}

	// shaping helpers
	static inline float applyPow(float u, float powAmt){
		// map powAmt [-1,1] to exponent (0.25..4]
		double t = (powAmt + 1.0) * 0.5;       // [0..1]
		double exp = 0.25 + t * (4.0 - 0.25);
		float x = std::clamp(u, 0.0f, 1.0f);
		return std::pow(x, (float)exp);
	}
	static inline float applyBiPow(float u, float biPowAmt){
		float x = u * 2.0f - 1.0f;
		double t = (biPowAmt + 1.0) * 0.5;     // [0..1]
		double exp = 0.25 + t * (4.0 - 0.25);
		float y = (x >= 0.0f) ? std::pow(x, (float)exp) : -std::pow(std::abs(x), (float)exp);
		return (y + 1.0f) * 0.5f;
	}
	static inline float applyQuant(float u, int steps){
		if(steps <= 0) return u;
		float s = (float)steps;
		return std::round(u * s) / s;
	}

	void generateNow(){
		int   N    = std::max(1, size_Param.get());
		float vmin = min_Param.get();
		float vmax = max_Param.get();
		float pw   = pow_Param.get();
		float bpw  = biPow_Param.get();
		int   q    = quant_Param.get();
		int   seed = seed_Param.get();

		std::vector<float> vec;
		vec.resize(N);

		if(seed == 0){
			// unseeded: different each trigger
			uint64_t t = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
			std::random_device rd;
			uint64_t base = ((uint64_t)rd() << 32) ^ rd() ^ t ^ (++unseededBump);
			uint32_t base32 = (uint32_t)(base ^ (base >> 32));
			for(int i=0;i<N;++i){
				uint32_t u = mixPair(base32, (uint32_t)i);
				float f = u32_to_unit(u);
				f = applyPow(f, pw);
				f = applyBiPow(f, bpw);
				f = applyQuant(f, q);
				vec[i] = ofMap(f, 0.0f, 1.0f, vmin, vmax, true);
			}
		} else {
			// seeded: deterministic per-index
			uint32_t s = (uint32_t)seed;
			for(int i=0;i<N;++i){
				uint32_t u = mixPair(s, (uint32_t)i);
				float f = u32_to_unit(u);
				f = applyPow(f, pw);
				f = applyBiPow(f, bpw);
				f = applyQuant(f, q);
				vec[i] = ofMap(f, 0.0f, 1.0f, vmin, vmax, true);
			}
		}

		output = vec;
		// keep output clamps aligned
		output.setMin(std::vector<float>(1, vmin));
		output.setMax(std::vector<float>(1, vmax));
	}
};
