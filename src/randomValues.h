// randomValues.h
#pragma once
#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <random>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cfloat>

class randomValues : public ofxOceanodeNodeModel {
public:
	randomValues() : ofxOceanodeNodeModel("Random Values") {}

	void setup() override {
		color = ofColor(0, 200, 255);
		description = "Generates a vector of random values when 'Generate' is triggered. "
			"Seed 0 is non-deterministic; a non-zero Seed produces a repeatable sequence "
			"that restarts when the Seed changes.";

		// Parameters
		addParameter(generateVoid.set("Generate"));
		
		addParameter(size_Param.set("Size", 16, 1, 4096));              // single-value size
		addParameter(min_Param.set("Min", 0.0f, -FLT_MAX,  FLT_MAX));
		addParameter(max_Param.set("Max", 1.0f, -FLT_MAX,  FLT_MAX));
		addParameter(pow_Param.set("Pow", 0.0f, -1.0f, 1.0f));          // unipolar curvature
		addParameter(biPow_Param.set("BiPow", 0.0f, -1.0f, 1.0f));      // bipolar curvature
		addParameter(quant_Param.set("Quant", 0, 0, INT_MAX));          // 0 = no quantization
		addParameter(seed_Param.set("Seed", 0, INT_MIN, INT_MAX));
		addInspectorParameter(legacyPowMode.set("Legacy Pow", true));

		addOutputParameter(output.set("Output", std::vector<float>{0.0f}, std::vector<float>{0.0f}, std::vector<float>{1.0f}));

		
		generateListener = generateVoid.newListener([this]{
			generateNow();
		});
		seedListener = seed_Param.newListener([this](int &){
			reseedGenerator();
		});

		// keep output clamps in sync
		minListener = min_Param.newListener([this](float &v){
			output.setMin(std::vector<float>(1, v));
		});
		maxListener = max_Param.newListener([this](float &v){
			output.setMax(std::vector<float>(1, v));
		});

		// Initialize the RNG before producing the initial vector.
		reseedGenerator();
		generateNow();
	}

private:
	// ---------- Parameters ----------
	ofParameter<int>    size_Param;
	ofParameter<float>  min_Param, max_Param;
	ofParameter<float>  pow_Param, biPow_Param;
	ofParameter<int>    quant_Param;
	ofParameter<int>    seed_Param;
	ofParameter<bool>   legacyPowMode;

	ofParameter<void>   generateVoid;

	// ---------- Output ----------
	ofParameter<std::vector<float>> output;

	// ---------- Listeners ----------
	ofEventListener generateListener;
	ofEventListener seedListener, minListener, maxListener;

	// ---------- RNG state ----------
	std::mt19937 generator;
	std::uniform_real_distribution<float> distribution{0.0f, 1.0f};

	// shaping helpers
	static inline float applyLegacyPow(float u, float powAmt){
		// Original Random Values mapping: [-1, 1] -> exponent [0.25, 4].
		const double t = (powAmt + 1.0) * 0.5;
		const double exponent = 0.25 + t * (4.0 - 0.25);
		const float x = std::clamp(u, 0.0f, 1.0f);
		return std::pow(x, static_cast<float>(exponent));
	}
	static inline float applyLegacyBiPow(float u, float biPowAmt){
		const float x = u * 2.0f - 1.0f;
		const double t = (biPowAmt + 1.0) * 0.5;
		const double exponent = 0.25 + t * (4.0 - 0.25);
		const float y = (x >= 0.0f)
			? std::pow(x, static_cast<float>(exponent))
			: -std::pow(std::abs(x), static_cast<float>(exponent));
		return (y + 1.0f) * 0.5f;
	}
	static inline float applyCustomPow(float u, float powAmt){
		if(powAmt == 0.0f) return u;
		const float k1 = 2.0f * powAmt * 0.99999f;
		const float k2 = k1 / ((-powAmt * 0.999999f) + 1.0f);
		const float k3 = k2 * std::abs(u) + 1.0f;
		return u * (k2 + 1.0f) / k3;
	}
	static inline float applyCustomBiPow(float u, float biPowAmt){
		if(biPowAmt == 0.0f) return u;
		float x = u * 2.0f - 1.0f;
		x = applyCustomPow(x, biPowAmt);
		return (x + 1.0f) * 0.5f;
	}
	static inline float applyQuant(float u, int steps){
		if(steps <= 0) return u;
		float s = (float)steps;
		return std::round(u * s) / s;
	}

	void reseedGenerator(){
		const int seed = seed_Param.get();
		if(seed == 0){
			std::random_device rd;
			std::seed_seq randomSeed{rd(), rd(), rd(), rd()};
			generator.seed(randomSeed);
		}else{
			generator.seed(static_cast<uint32_t>(seed));
		}
	}

	void generateNow(){
		int   N    = std::max(1, size_Param.get());
		float vmin = min_Param.get();
		float vmax = max_Param.get();
		float pw   = pow_Param.get();
		float bpw  = biPow_Param.get();
		int   q    = quant_Param.get();
		bool  legacyPow = legacyPowMode.get();

		std::vector<float> vec;
		vec.resize(N);

		for(int i=0;i<N;++i){
			float f = distribution(generator);
			if(legacyPow){
				f = applyLegacyPow(f, pw);
				f = applyLegacyBiPow(f, bpw);
			}else{
				f = applyCustomPow(f, pw);
				f = applyCustomBiPow(f, bpw);
			}
			f = applyQuant(f, q);
			vec[i] = ofMap(f, 0.0f, 1.0f, vmin, vmax, true);
		}

		output = vec;
		// keep output clamps aligned
		output.setMin(std::vector<float>(1, vmin));
		output.setMax(std::vector<float>(1, vmax));
	}
};
