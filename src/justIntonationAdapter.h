#ifndef justIntonationAdapter_h
#define justIntonationAdapter_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <cfloat>
#include <algorithm>

class justIntonationAdapter : public ofxOceanodeNodeModel {
public:
	justIntonationAdapter() : ofxOceanodeNodeModel("Just Intonation Adapter") {}

	void setup() override {
		description = "Converts 12-TET semitone offsets into 'just-corrected' semitones. "
					  "Input is a vector of semitones (e.g. 0,4,7). Output is a vector "
					  "of floats such that 2^(out/12) follows the selected tuning "
					  "system (5-limit, 7-limit, 11-limit, Pythagorean, Custom, or "
					  "NearestHarmonic).";

		// Input: 12-TET semitone offsets
		addParameter(semitones.set("Semitones",
								   std::vector<float>{0.0f, 4.0f, 7.0f},
								   std::vector<float>{-FLT_MAX},
								   std::vector<float>{FLT_MAX}));

		// Mapping mode
		// 0 = Bypass           (no correction)
		// 1 = 5-limit JI
		// 2 = 7-limit JI
		// 3 = 11-limit JI
		// 4 = Pythagorean (pure P5)
		// 5 = Custom (uses customRatios)
		// 6 = NearestHarmonic (snap to nearest harmonic 1..maxHarmonic)
		addParameterDropdown(mappingMode,
							 "Mode",
							 0,
							 {
								 "Bypass",
								 "5-limit",
								 "7-limit",
								 "11-limit",
								 "PythagoreanP5",
								 "Custom",
								 "NearestHarmonic"
							 });

		// Max harmonic for NearestHarmonic mode
		addParameter(maxHarmonic.set("MaxHarmonic", 32, 1, 128));

		// Custom ratio mapping within the octave.
		// Index = degree (0..n-1), used modulo size for any degree.
		// If empty in Custom mode, the node behaves like Bypass.
		// IMPORTANT: default has at least one element so GUI doesn't crash.
		addParameter(customRatios.set("CustomRatios",
									  std::vector<float>{1.0f},
									  std::vector<float>{-FLT_MAX},
									  std::vector<float>{FLT_MAX}));

		// Output: JI-corrected semitone offsets
		addOutputParameter(jiSemitones.set("JI Semitones",
										   std::vector<float>{0.0f},
										   std::vector<float>{-FLT_MAX},
										   std::vector<float>{FLT_MAX}));

		// Listeners
		listeners.push(semitones.newListener([this](std::vector<float> &) { recompute(); }));
		listeners.push(mappingMode.newListener([this](int &) { recompute(); }));
		listeners.push(customRatios.newListener([this](std::vector<float> &) { recompute(); }));
		listeners.push(maxHarmonic.newListener([this](int &) { recompute(); }));

		recompute();
	}

private:
	// --- Helper: safe log2 ---
	float log2f_safe(float x) {
		return std::log(x) / std::log(2.0f);
	}

	// --- Helper: 5-limit mapping 0..11 ---
	float ratio5Limit(int deg) {
		static const float table[12] = {
			1.0f,               // 0: 1/1
			16.0f / 15.0f,      // 1: m2-ish
			9.0f  / 8.0f,       // 2: M2
			6.0f  / 5.0f,       // 3: m3
			5.0f  / 4.0f,       // 4: M3
			4.0f  / 3.0f,       // 5: P4
			std::sqrt(2.0f),    // 6: tritone-ish
			3.0f  / 2.0f,       // 7: P5
			8.0f  / 5.0f,       // 8: m6
			5.0f  / 3.0f,       // 9: M6
			9.0f  / 5.0f,       //10: m7
			15.0f / 8.0f        //11: M7
		};
		deg = deg % 12;
		if(deg < 0) deg += 12;
		return table[deg];
	}

	// --- Helper: 7-limit mapping 0..11 ---
	// For now: 5-limit, but degree 10 (minor 7th) → 7/4.
	float ratio7Limit(int deg) {
		deg = deg % 12;
		if(deg < 0) deg += 12;

		switch(deg){
			case 10:
				return 7.0f / 4.0f; // harmonic 7th
			default:
				return ratio5Limit(deg);
		}
	}

	// --- Helper: 11-limit mapping 0..11 ---
	// For now: 5-limit, but degree 5 (P4) → 11/8 to give an 11th-ish color.
	float ratio11Limit(int deg) {
		deg = deg % 12;
		if(deg < 0) deg += 12;

		switch(deg){
			case 5:
				return 11.0f / 8.0f; // 11th harmonic flavor
			default:
				return ratio5Limit(deg);
		}
	}

	// --- Helper: Pythagorean mapping 0..11 ---
	// Precomputed from stacking pure fifths (3/2) and reducing to [1,2).
	float ratioPythagorean(int deg) {
		static const float table[12] = {
			1.0f,                  // 0
			2187.0f / 2048.0f,     // 1
			9.0f   / 8.0f,         // 2
			19683.0f / 16384.0f,   // 3
			81.0f  / 64.0f,        // 4
			177147.0f / 131072.0f, // 5
			729.0f / 512.0f,       // 6
			3.0f   / 2.0f,         // 7
			6561.0f / 4096.0f,     // 8
			27.0f  / 16.0f,        // 9
			59049.0f / 32768.0f,   //10
			243.0f  / 128.0f       //11
		};
		deg = deg % 12;
		if(deg < 0) deg += 12;
		return table[deg];
	}

	// --- Helper: Custom mapping 0..11 ---
	float ratioCustom(int deg, const std::vector<float> &custom) {
		if(custom.empty()){
			// treated as bypass higher up
			return 1.0f;
		}
		deg = deg % 12;
		if(deg < 0) deg += 12;

		int idx = deg % static_cast<int>(custom.size());
		return custom[idx];
	}

	// --- Helper: Nearest harmonic (1..maxHarmonic) in the octave ---
	// 0 is treated as 1/1. We ignore absolute root Hz; we just
	// assume degree 0 = 1/1 and snap deg to nearest harmonic
	// when both are octave-normalized.
	float ratioNearestHarmonic(int deg) {
		deg = deg % 12;
		if(deg < 0) deg += 12;

		// Equal-tempered ratio for this degree within the octave
		float rET = std::pow(2.0f, static_cast<float>(deg) / 12.0f);

		float bestRatio = 1.0f;
		float bestErr   = FLT_MAX;

		int maxN = std::max(1, maxHarmonic.get());

		for(int n = 1; n <= maxN; ++n){
			float nf = static_cast<float>(n);

			// Normalize harmonic n into [1,2)
			int oct = 0;
			if(nf >= 1.0f){
				oct = static_cast<int>(std::floor(log2f_safe(nf)));
			}
			float r_n = nf / std::pow(2.0f, static_cast<float>(oct));

			// Compare in log2 domain (octaves) for scale invariance
			float err = std::fabs(log2f_safe(r_n / rET));
			if(err < bestErr){
				bestErr   = err;
				bestRatio = r_n;
			}
		}

		return bestRatio;
	}

	void recompute() {
		const std::vector<float> &in = semitones.get();
		int mode = mappingMode.get();

		// If input semitones is empty → empty output
		if(in.empty()) {
			jiSemitones = std::vector<float>{};
			return;
		}

		const std::vector<float> &custom = customRatios.get();
		bool customEmpty = custom.empty();
		bool treatAsBypass = (mode == 0) || (mode == 5 && customEmpty);

		// Bypass (explicitly or Custom with empty customRatios)
		if(treatAsBypass){
			jiSemitones = in;
			return;
		}

		std::vector<float> out;
		out.resize(in.size());

		for(size_t i = 0; i < in.size(); ++i){
			float s = in[i];

			// Map to nearest integer semitone for degree selection
			int n = static_cast<int>(std::floor(s + 0.5f));

			// Compute octave and degree (0..11)
			int oct, deg;
			if(n >= 0) {
				oct = n / 12;
				deg = n % 12;
			}else{
				// floor-style for negative
				oct = (n - 11) / 12;
				deg = n - oct * 12;
			}

			// Get within-octave ratio according to mapping mode
			float baseRatio = 1.0f;

			switch(mode){
				case 1: // 5-limit
					baseRatio = ratio5Limit(deg);
					break;
				case 2: // 7-limit
					baseRatio = ratio7Limit(deg);
					break;
				case 3: // 11-limit
					baseRatio = ratio11Limit(deg);
					break;
				case 4: // Pythagorean
					baseRatio = ratioPythagorean(deg);
					break;
				case 5: // Custom
					baseRatio = ratioCustom(deg, custom);
					break;
				case 6: // Nearest harmonic 1..maxHarmonic
					baseRatio = ratioNearestHarmonic(deg);
					break;
				default:
					baseRatio = 1.0f;
					break;
			}

			// Apply octave
			float ratioTotal = baseRatio * std::pow(2.0f, static_cast<float>(oct));

			// Convert that ratio back to a "virtual semitone" value
			float s_ji = 12.0f * log2f_safe(ratioTotal);

			out[i] = s_ji;
		}

		jiSemitones = out;
	}

	// Parameters
	ofParameter<std::vector<float>> semitones;
	ofParameter<int>                mappingMode;
	ofParameter<int>                maxHarmonic;
	ofParameter<std::vector<float>> customRatios;
	ofParameter<std::vector<float>> jiSemitones;

	ofEventListeners listeners;
};

#endif /* justIntonationAdapter_h */
