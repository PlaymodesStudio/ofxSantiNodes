#ifndef increment_h
#define increment_h

#include "ofxOceanodeNodeModel.h"

class increment : public ofxOceanodeNodeModel {
public:
	increment() : ofxOceanodeNodeModel("Increment") {}

	void setup() override {
		description = "Increments or decrements a value by a step amount. Output resets to input value when input changes.";
		
		// Input parameters
		addParameter(input.set("Input", 0.0f, -FLT_MAX, FLT_MAX));
		addParameter(step.set("Step", 1.0f, -FLT_MAX, FLT_MAX));
		
		// Button parameters (void type)
		addParameter(incrementButton.set("Increment"));
		addParameter(decrementButton.set("Decrement"));
		
		// Output parameter
		addOutputParameter(output.set("Output", 0.0f, -FLT_MAX, FLT_MAX));
		
		// Initialize internal value
		internalValue = input.get();
		output.set(internalValue);
		
		// Listeners
		listeners.push(input.newListener([this](float &value){
			// When input changes, reset internal value and output to the new input value
			internalValue = value;
			output.set(internalValue);
		}));
		
		listeners.push(incrementButton.newListener([this](){
			// Increment internal value by step amount
			internalValue += step.get();
			output.set(internalValue);
		}));
		
		listeners.push(decrementButton.newListener([this](){
			// Decrement internal value by step amount
			internalValue -= step.get();
			output.set(internalValue);
		}));
	}

private:
	// Parameters
	ofParameter<float> input;
	ofParameter<float> step;
	ofParameter<void> incrementButton;
	ofParameter<void> decrementButton;
	ofParameter<float> output;
	
	// Internal state
	float internalValue;
	
	ofEventListeners listeners;
};

#endif /* increment_h */
