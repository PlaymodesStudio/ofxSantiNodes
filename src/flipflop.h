#ifndef flipflop_h
#define flipflop_h

#include "ofxOceanodeNodeModel.h"

class flipflop : public ofxOceanodeNodeModel {
public:
	flipflop() : ofxOceanodeNodeModel("Flip Flop") {}

	void setup() override {
		description = "Toggles between 0 and 1 based on input triggers. Input '1' sets output to 1, input '0' sets output to 0, input '1-0' toggles between 1 and 0.";
		
		// Add the void input parameters with distinctive names
		addParameter(setToOne.set("1"));
		addParameter(setToZero.set("0"));
		addParameter(toggle.set("1-0"));
		
		// Add the integer output parameter
		addOutputParameter(output.set("Output", 0, 0, 1));
		
		// Add event listeners for the void inputs
		setToOneListener = setToOne.newListener([this](){
			output = 1;
		});
		
		setToZeroListener = setToZero.newListener([this](){
			output = 0;
		});
		
		toggleListener = toggle.newListener([this](){
			// Flip between 0 and 1
			output = 1 - output;
		});
	}

private:
	// Input parameters (void type)
	ofParameter<void> setToOne;
	ofParameter<void> setToZero;
	ofParameter<void> toggle;
	
	// Output parameter (int type)
	ofParameter<int> output;
	
	// Event listeners
	ofEventListener setToOneListener;
	ofEventListener setToZeroListener;
	ofEventListener toggleListener;
};

#endif /* flipflop_h */
