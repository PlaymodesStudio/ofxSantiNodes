#ifndef intervalRatios_h
#define intervalRatios_h

#include "ofxOceanodeNodeModel.h"
#include <cmath>
#include <cfloat>

class intervalRatios : public ofxOceanodeNodeModel {
public:
	intervalRatios() : ofxOceanodeNodeModel("Interval Ratios") {}

	void setup() override {
		description = "Outputs frequency ratios for musical intervals. "
					  "Can work in Just Intonation or 12-TET. "
					  "Useful as a Step source for progression nodes.";

		// Dropdown d'intervals
		// Ordre:
		// 0 = Unison
		// 1 = minor 2nd
		// 2 = major 2nd
		// 3 = minor 3rd
		// 4 = major 3rd
		// 5 = perfect 4th
		// 6 = tritone
		// 7 = perfect 5th
		// 8 = minor 6th
		// 9 = major 6th
		// 10 = minor 7th
		// 11 = major 7th
		// 12 = octave
		addParameterDropdown(interval,
							 "Interval",
							 7, // default: P5
							 {
								 "Unison",
								 "m2",
								 "M2",
								 "m3",
								 "M3",
								 "P4",
								 "Tritone",
								 "P5",
								 "m6",
								 "M6",
								 "m7",
								 "M7",
								 "Octave"
							 });

		// Sistema d'afinació
		// 0 = Just Intonation
		// 1 = 12-TET
		addParameterDropdown(tuning,
							 "Tuning",
							 0, // default: Just
							 {
								 "Just",
								 "12-TET"
							 });

		// Sortida: ratio
		addOutputParameter(output.set("Output", 1.0f, 0.0f, FLT_MAX));

		// Listeners
		listeners.push(interval.newListener([this](int &) {
			recompute();
		}));
		listeners.push(tuning.newListener([this](int &) {
			recompute();
		}));

		recompute();
	}

private:
	void recompute() {
		int idx = ofClamp(interval.get(), 0, 12);
		int tun = tuning.get();

		// Semitons per cada interval (respecte a la tònica)
		static const int semitonesTable[13] = {
			0,  // Unison
			1,  // m2
			2,  // M2
			3,  // m3
			4,  // M3
			5,  // P4
			6,  // Tritone
			7,  // P5
			8,  // m6
			9,  // M6
			10, // m7
			11, // M7
			12  // Octave
		};

		// Just intonation ratios aproximats
		static const float justTable[13] = {
			1.0f,              // Unison  1/1
			16.0f / 15.0f,     // m2
			9.0f / 8.0f,       // M2
			6.0f / 5.0f,       // m3
			5.0f / 4.0f,       // M3
			4.0f / 3.0f,       // P4
			std::sqrt(2.0f),   // Tritone ~ sqrt(2)
			3.0f / 2.0f,       // P5
			8.0f / 5.0f,       // m6
			5.0f / 3.0f,       // M6
			9.0f / 5.0f,       // m7
			15.0f / 8.0f,      // M7
			2.0f               // Octave 2/1
		};

		float ratio = 1.0f;

		if(tun == 0){
			// Just
			ratio = justTable[idx];
		}else{
			// 12-TET
			int st = semitonesTable[idx];
			ratio = std::pow(2.0f, static_cast<float>(st) / 12.0f);
		}

		output.set(ratio);
	}

	ofParameter<int>   interval;
	ofParameter<int>   tuning;
	ofParameter<float> output;

	ofEventListeners listeners;
};

#endif /* intervalRatios_h */
