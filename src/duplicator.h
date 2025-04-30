#ifndef duplicator_h
#define duplicator_h

#include "ofxOceanodeNodeModel.h"

class duplicator : public ofxOceanodeNodeModel {
public:
	duplicator() : ofxOceanodeNodeModel("Duplicator") {}

	void setup() override {
		description = "Duplicates each value in the input vector the number of times specified in the duplicate nums vector.";
		
		// Add input parameters
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(duplicateNums.set("DupNums", {1}, {0}, {INT_MAX}));
		
		// Add output parameter
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Add listeners to process when inputs change
		listeners.push(input.newListener([this](vector<float> &vf){
			processVectors();
		}));
		
		listeners.push(duplicateNums.newListener([this](vector<int> &vi){
			processVectors();
		}));
	}

private:
	void processVectors() {
		// Get references to the input vectors
		const auto& inputVec = input.get();
		const auto& dupVec = duplicateNums.get();

		// Create result vector
		vector<float> result;

		// Handle single duplication value case (broadcast)
		int duplicateCount = dupVec[0];
		if (dupVec.size() == 1) {
			// Reserve space to avoid reallocations
			result.reserve(inputVec.size() * duplicateCount);
			
			// Apply the same duplication count to all input values
			for (size_t i = 0; i < inputVec.size(); i++) {
				for (int j = 0; j < duplicateCount; j++) {
					result.push_back(inputVec[i]);
				}
			}
		}
		// Handle per-element duplication case
		else {
			// Process only up to the size of the smaller vector
			size_t minSize = std::min(inputVec.size(), dupVec.size());
			
			// Reserve approximate space to avoid reallocations
			size_t totalSize = 0;
			for (size_t i = 0; i < minSize; i++) {
				totalSize += std::max(0, dupVec[i]); // Ensure we don't add negative counts
			}
			result.reserve(totalSize);
			
			// Duplicate each value according to the duplicate nums vector
			for (size_t i = 0; i < minSize; i++) {
				int repeats = std::max(0, dupVec[i]); // Ensure non-negative repeat count
				for (int j = 0; j < repeats; j++) {
					result.push_back(inputVec[i]);
				}
			}
		}

		// Update output
		output = result;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<int>> duplicateNums;
	ofParameter<vector<float>> output;
	
	ofEventListeners listeners;
};

#endif /* duplicator_h */
