#ifndef vectorFold_h
#define vectorFold_h

#include "ofxOceanodeNodeModel.h"

class vectorFold : public ofxOceanodeNodeModel {
public:
	vectorFold() : ofxOceanodeNodeModel("Vector Fold") {}

	void setup() override {
		description = "Folds a larger input vector into a smaller output vector by wrapping and summing values.";
		
		// Input parameters
		addParameter(inputVector.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(outputSize.set("Out Size", 2, 1, 100));
		
		// Output parameter
		addOutputParameter(outputVector.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Add listeners
		listener = inputVector.newListener([this](vector<float> &){
			processVectorFold();
		});
		
		sizeListener = outputSize.newListener([this](int &){
			processVectorFold();
		});
	}

private:
	void processVectorFold() {
		const vector<float>& input = inputVector.get();
		int outSize = outputSize.get();
		
		// Ensure output size is at least 1
		outSize = std::max(1, outSize);
		
		// Initialize output vector with zeros
		vector<float> result(outSize, 0.0f);
		
		// Process each input value
		for(size_t i = 0; i < input.size(); ++i) {
			// Determine which output index to add to
			int outputIndex = i % outSize;
			
			// Add the input value to the appropriate output element
			result[outputIndex] += input[i];
		}
		
		// Set the output parameter
		outputVector = result;
	}

	ofParameter<vector<float>> inputVector;
	ofParameter<int> outputSize;
	ofParameter<vector<float>> outputVector;
	ofEventListener listener;
	ofEventListener sizeListener;
};

#endif /* vectorFold_h */
