// randomValues.h
#pragma once
#include "ofxOceanodeNodeModel.h"
// Transport mode needs ofxOceanode's global transport (feature-globalTransport branch).
// Without it the node compiles exactly as before.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
#include "ofxOceanodeDeterministicRandom.h"
#endif
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

		
		// Sync To Transport: output is a pure function of (Seed, Step, params), regenerated
		// whenever any of them changes. Connect Step to a Phasor "Cycle" output so any
		// timeline position (play, scrub, seek) gives the same values.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
		sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
		addInspectorParameter(syncToTransport_Param.set("Sync To Transport", false));
#endif

		generateListener = generateVoid.newListener([this]{
			if(isTransportMode()) generateTransport(false);
			else generateNow();
		});
		seedListener = seed_Param.newListener([this](int &){
			if(isTransportMode()) generateTransport(false);
			else reseedGenerator();
		});

		// keep output clamps in sync
		minListener = min_Param.newListener([this](float &v){
			output.setMin(std::vector<float>(1, v));
			transportParamChanged();
		});
		maxListener = max_Param.newListener([this](float &v){
			output.setMax(std::vector<float>(1, v));
			transportParamChanged();
		});
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
		transportListeners.push(size_Param.newListener([this](int &){ transportParamChanged(); }));
		transportListeners.push(pow_Param.newListener([this](float &){ transportParamChanged(); }));
		transportListeners.push(biPow_Param.newListener([this](float &){ transportParamChanged(); }));
		transportListeners.push(quant_Param.newListener([this](int &){ transportParamChanged(); }));
		transportListeners.push(legacyPowMode.newListener([this](bool &){ transportParamChanged(); }));
		transportListeners.push(step_Param.newListener([this](std::vector<float> &){
			if(syncToTransport_Param) generateTransport(false);
		}));
		transportListeners.push(syncToTransport_Param.newListener([this](bool &b){
			setStepInputVisible(b);
			if(b) generateTransport(true);
			else reseedGenerator();
		}));
#endif

		// Initialize the RNG before producing the initial vector.
		reseedGenerator();
		generateNow();
	}

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	void loadBeforeConnections(ofJson &json) override {
		// Restore the mode before connections so a saved "Step" connection finds its input.
		deserializeParameter(json, syncToTransport_Param);
	}
#endif

private:
	// ---------- Transport mode ----------
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
	bool isTransportMode() const { return syncToTransport_Param.get(); }
	ofParameter<bool> syncToTransport_Param;
	ofParameter<std::vector<float>> step_Param; // only present in Sync To Transport mode
	ofEventListeners transportListeners;
	uint64_t sessionSalt = 0;
	bool hasTransportCache = false;
	int64_t lastTransportStep = 0;
	uint64_t lastTransportSeedKey = 0;

	void setStepInputVisible(bool visible){
		const bool present = getParameterGroup().contains("Step");
		if(visible && !present){
			addParameter(step_Param.set("Step", std::vector<float>{0.0f}, std::vector<float>{0.0f}, std::vector<float>{FLT_MAX}));
		}else if(!visible && present){
			getOceanodeParameter(step_Param).removeAllConnections();
			removeParameter("Step");
		}
	}

	void transportParamChanged(){
		if(syncToTransport_Param) generateTransport(true);
	}

	void generateTransport(bool force){
		const auto &steps = step_Param.get();
		const int64_t step = steps.empty() ? 0 : ofxOceanodeDeterministicRandom::stepFromFloat(steps[0]);
		const uint64_t key = ofxOceanodeDeterministicRandom::seedKey(seed_Param.get(), sessionSalt);
		// Skip redundant regeneration (e.g. Generate still wired to a phasor Reset).
		if(!force && hasTransportCache && step == lastTransportStep && key == lastTransportSeedKey) return;
		hasTransportCache = true;
		lastTransportStep = step;
		lastTransportSeedKey = key;

		const int N = std::max(1, size_Param.get());
		std::vector<float> vec(N);
		for(int i = 0; i < N; ++i){
			// Element i uses its own stream, so Size changes don't reshuffle other elements.
			vec[i] = shapeValue(ofxOceanodeDeterministicRandom::uniform(key, step, static_cast<uint64_t>(i)));
		}
		output = vec;
		output.setMin(std::vector<float>(1, min_Param.get()));
		output.setMax(std::vector<float>(1, max_Param.get()));
	}
#else
	bool isTransportMode() const { return false; }
	void transportParamChanged(){}
	void generateTransport(bool){}
#endif

	float shapeValue(float f){
		const float pw = pow_Param.get();
		const float bpw = biPow_Param.get();
		if(legacyPowMode.get()){
			f = applyLegacyPow(f, pw);
			f = applyLegacyBiPow(f, bpw);
		}else{
			f = applyCustomPow(f, pw);
			f = applyCustomBiPow(f, bpw);
		}
		f = applyQuant(f, quant_Param.get());
		return ofMap(f, 0.0f, 1.0f, min_Param.get(), max_Param.get(), true);
	}

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

		std::vector<float> vec;
		vec.resize(N);

		for(int i=0;i<N;++i){
			vec[i] = shapeValue(distribution(generator));
		}

		output = vec;
		// keep output clamps aligned
		output.setMin(std::vector<float>(1, vmin));
		output.setMax(std::vector<float>(1, vmax));
	}
};
