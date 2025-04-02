#ifndef vectorSampler_h
#define vectorSampler_h

#include "ofxOceanodeNodeModel.h"
#include <cmath>

class vectorSampler : public ofxOceanodeNodeModel {
public:
	enum RoundingMode {
		FLOOR = 0,
		CEILING = 1,
		ROUND = 2,
		TRUNCATE = 3,
		NEAREST_EVEN = 4,
		INTERPOLATE = 5
	};
	
	vectorSampler() : ofxOceanodeNodeModel("Vector Sampler") {}

	void setup() override {
		description = "Samples n evenly-spaced elements from the input vector.";
		
		// Input parameters
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(numSamples.set("N", 2, 1, 100));
		
		// Add rounding mode dropdown
		vector<string> roundingOptions = {
			"Floor", "Ceiling", "Round", "Truncate", "Nearest Even", "Interpolate"
		};
		addParameterDropdown(roundingMode, "Mode", 0, roundingOptions);
		
		// Output parameter
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Add listeners to recalculate when input parameters change
		listeners.push(input.newListener([this](vector<float> &){
			recalculate();
		}));
		
		listeners.push(numSamples.newListener([this](int &){
			recalculate();
		}));
		
		listeners.push(roundingMode.newListener([this](int &){
			recalculate();
		}));
	}
	
private:
	void recalculate() {
		const auto& inputVec = input.get();
		int size = inputVec.size();
		int n = numSamples.get();
		
		// Handle edge cases
		if (size == 0 || n <= 0) {
			output.set({});
			return;
		}
		
		if (n == 1) {
			// If only one sample is requested, return the first element
			output.set({inputVec[0]});
			return;
		}
		
		// Create output vector
		vector<float> result;
		result.reserve(n);
		
		// For interpolation we need to handle differently
		if (roundingMode.get() == INTERPOLATE) {
			for (int i = 0; i < n; i++) {
				float position = i * (size - 1.0f) / (n - 1.0f);
				int index1 = floor(position);
				int index2 = ceil(position);
				
				if (index1 == index2) {
					// Exact index
					result.push_back(inputVec[index1]);
				} else {
					// Interpolate between two adjacent values
					float weight2 = position - index1;
					float weight1 = 1.0f - weight2;
					float value = weight1 * inputVec[index1] + weight2 * inputVec[index2];
					result.push_back(value);
				}
			}
		} else {
			// For all other rounding modes
			for (int i = 0; i < n; i++) {
				float position = i * (size - 1.0f) / (n - 1.0f);
				int index = 0;
				
				switch (roundingMode.get()) {
					case FLOOR:
						index = floor(position);
						break;
					case CEILING:
						index = ceil(position);
						break;
					case ROUND:
						index = round(position);
						break;
					case TRUNCATE:
						index = trunc(position);
						break;
					case NEAREST_EVEN:
						// Banker's rounding (round to nearest even when exactly halfway)
						if (position - floor(position) == 0.5f) {
							int floored = static_cast<int>(floor(position));
							index = (floored % 2 == 0) ? floored : ceil(position);
						} else {
							index = round(position);
						}
						break;
				}
				
				// Ensure index is within bounds
				index = std::max(0, std::min(size - 1, index));
				result.push_back(inputVec[index]);
			}
		}
		
		output.set(result);
	}
	
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<int> numSamples;
	ofParameter<int> roundingMode;
	ofEventListeners listeners;
};

#endif /* vectorSampler_h */
