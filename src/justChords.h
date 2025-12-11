#ifndef justChords_h
#define justChords_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cmath>
#include <cfloat>
#include <algorithm>

class justChords : public ofxOceanodeNodeModel {
public:
	justChords() : ofxOceanodeNodeModel("Just Intonation Chords") {}

	void setup() override {
		description = "Builds chords by stacking an arbitrary semitone-step pattern "
					  "on top of a root. The Step parameter is a vector (e.g. {3,4} "
					  "for a minor-like stack, {5} for quartal, {7} for quintal). "
					  "Length controls how many notes are generated. Supports Just "
					  "Intonation and 12-TET.";

		// Root: can be 1.0 (ratio) or a frequency in Hz
		addParameter(root.set("Root", 1.0f, 0.0f, FLT_MAX));

		// Tuning: Just or 12-TET
		addParameterDropdown(tuning,
							 "Tuning",
							 0,  // default: Just
							 {
								 "Just",
								 "12-TET"
							 });

		// Step pattern in semitones (vector)
		// Example patterns:
		//  {4,3} = major-ish tertian
		//  {3,4} = minor-ish tertian
		//  {3,3} = diminished-like
		//  {4,4} = augmented-like
		//  {5}   = quartal
		//  {7}   = quintal
		addParameter(stepPattern.set("Step",
									 std::vector<float>{4.0f, 3.0f},
									 std::vector<float>{-48.0f},
									 std::vector<float>{48.0f}));

		// Length of chord (number of notes)
		addParameter(length.set("Length", 3, 1, 32)); // 1..32 notes

		// Output: chord tones
		addOutputParameter(output.set("Output",
									  std::vector<float>{1.0f},
									  std::vector<float>{0.0f},
									  std::vector<float>{FLT_MAX}));

		// Listeners
		listeners.push(root.newListener([this](float &) { recompute(); }));
		listeners.push(tuning.newListener([this](int &) { recompute(); }));
		listeners.push(stepPattern.newListener([this](std::vector<float> &) { recompute(); }));
		listeners.push(length.newListener([this](int &) { recompute(); }));

		recompute();
	}

private:
	// Map 0–11 semitone offsets to approximate Just ratios and extend with octaves.
	float semitoneToJustRatio(int semitones) {
		static const float baseTable[12] = {
			1.0f,              // 0: unison
			16.0f / 15.0f,     // 1: ~ m2
			9.0f  / 8.0f,      // 2: M2
			6.0f  / 5.0f,      // 3: m3
			5.0f  / 4.0f,      // 4: M3
			4.0f  / 3.0f,      // 5: P4
			std::sqrt(2.0f),   // 6: tritone-ish
			3.0f  / 2.0f,      // 7: P5
			8.0f  / 5.0f,      // 8: m6
			5.0f  / 3.0f,      // 9: M6
			9.0f  / 5.0f,      //10: m7
			15.0f / 8.0f       //11: M7
		};

		int octaves = 0;
		int rem     = semitones;

		if(rem >= 0){
			octaves = rem / 12;
			rem     = rem % 12;
		}else{
			// for negative semitones, floor-style division
			octaves = (rem - 11) / 12;
			rem     = rem - octaves * 12;
		}

		float base = baseTable[rem];
		return base * std::pow(2.0f, static_cast<float>(octaves));
	}

	void recompute() {
		float r   = root.get();
		int tun   = tuning.get();
		int len   = std::max(1, length.get());
		const std::vector<float> &stepsF = stepPattern.get();

		if(stepsF.empty()){
			// If no steps, just output the root
			std::vector<float> chord(1, r);
			output = chord;
			return;
		}

		// Convert step pattern to integer semitones (rounded)
		std::vector<int> steps;
		steps.reserve(stepsF.size());
		for(float s : stepsF){
			steps.push_back(static_cast<int>(std::round(s)));
		}

		// Build semitone offsets from step pattern
		std::vector<int> semis;
		semis.resize(len);
		semis[0] = 0;

		for(int i = 1; i < len; ++i){
			int patternIndex = (i - 1) % steps.size();
			semis[i] = semis[i - 1] + steps[patternIndex];
		}

		// Convert semitone offsets to actual frequencies/ratios
		std::vector<float> chord;
		chord.resize(len);

		for(int i = 0; i < len; ++i){
			int st = semis[i];
			float ratio;

			if(tun == 0) {
				// Just intonation
				ratio = semitoneToJustRatio(st);
			} else {
				// 12-TET
				ratio = std::pow(2.0f, static_cast<float>(st) / 12.0f);
			}

			chord[i] = r * ratio;
		}

		output = chord;
	}

	// Parameters
	ofParameter<float>              root;
	ofParameter<int>                tuning;
	ofParameter<std::vector<float>> stepPattern;
	ofParameter<int>                length;

	ofParameter<std::vector<float>> output;

	ofEventListeners listeners;
};

#endif /* justChords_h */
