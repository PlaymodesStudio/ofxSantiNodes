#ifndef trigonometry_h
#define trigonometry_h

#include "ofxOceanodeNodeModel.h"
#include <float.h>
#include <cmath>

class trigonometry : public ofxOceanodeNodeModel {
public:
	trigonometry() : ofxOceanodeNodeModel("Trigonometry") {}
	
	void setup() override {
		description = "Performs various trigonometric operations on each element of the input vector.";
		
		// Define trigonometric operations
		std::vector<std::string> operationNames = {
			"Sine", "Cosine", "Tangent",
			"Arc Sine", "Arc Cosine", "Arc Tangent",
			"Hyperbolic Sine", "Hyperbolic Cosine", "Hyperbolic Tangent",
			"Degrees to Radians", "Radians to Degrees"
		};
		
		// Add parameters
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameterDropdown(operationSelector, "Operation", 0, operationNames);
		addParameter(useDegrees.set("Use Degrees", false));
		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		
		// Setup listeners to trigger processing when parameters change
		inputListener = input.newListener([this](vector<float> &_){
			processOperation();
		});
		
		operationListener = operationSelector.newListener([this](int &_){
			processOperation();
		});
		
		useDegreesListener = useDegrees.newListener([this](bool &_){
			processOperation();
		});
	}
	
private:
	void processOperation() {
		auto inputValues = input.get();
		vector<float> outputValues;
		outputValues.reserve(inputValues.size());
		
		// Process each value based on selected operation
		for (float value : inputValues) {
			float result = 0.0f;
			
			// Convert degrees to radians if needed for trig functions
			float processedValue = value;
			if (useDegrees && operationSelector < 9 && operationSelector != 9) {
				processedValue = value * DEG_TO_RAD;
			}
			
			// Apply the selected operation
			switch (operationSelector) {
				case 0: // Sine
					result = sin(processedValue);
					break;
					
				case 1: // Cosine
					result = cos(processedValue);
					break;
					
				case 2: // Tangent
					result = tan(processedValue);
					break;
					
				case 3: // Arc Sine
					// Clamp input to valid range [-1, 1]
					processedValue = std::max(-1.0f, std::min(1.0f, value));
					result = asin(processedValue);
					// Convert back to degrees if needed
					if (useDegrees) result *= RAD_TO_DEG;
					break;
					
				case 4: // Arc Cosine
					// Clamp input to valid range [-1, 1]
					processedValue = std::max(-1.0f, std::min(1.0f, value));
					result = acos(processedValue);
					// Convert back to degrees if needed
					if (useDegrees) result *= RAD_TO_DEG;
					break;
					
				case 5: // Arc Tangent
					result = atan(value);
					// Convert back to degrees if needed
					if (useDegrees) result *= RAD_TO_DEG;
					break;
					
				case 6: // Hyperbolic Sine
					result = sinh(processedValue);
					break;
					
				case 7: // Hyperbolic Cosine
					result = cosh(processedValue);
					break;
					
				case 8: // Hyperbolic Tangent
					result = tanh(processedValue);
					break;
				
				case 9: // Degrees to Radians
					result = value * DEG_TO_RAD;
					break;
					
				case 10: // Radians to Degrees
					result = value * RAD_TO_DEG;
					break;
					
				default:
					result = value;
					break;
			}
			
			outputValues.push_back(result);
		}
		
		output = outputValues;
	}
	
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	ofParameter<int> operationSelector;
	ofParameter<bool> useDegrees;
	ofEventListener inputListener;
	ofEventListener operationListener;
	ofEventListener useDegreesListener;
};

#endif /* trigonometry_h */
