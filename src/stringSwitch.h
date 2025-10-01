#ifndef stringSwitch_h
#define stringSwitch_h

#include "ofxOceanodeNodeModel.h"

class stringSwitch : public ofxOceanodeNodeModel {
public:
	stringSwitch() : ofxOceanodeNodeModel("String Switch") {}

	void setup() override {
		description = "Switches between multiple string inputs based on switch parameter. Number of inputs is configurable in inspector.";
		
		// Inspector parameter for number of inputs
		addInspectorParameter(numInputs.set("Num Inputs", 2, 1, 16));
		
		// Switch parameter to select active input
		addParameter(switchParam.set("Switch", 0, 0, 1));
		
		// Output parameter
		addOutputParameter(output.set("Output", ""));
		
		// Initialize with default number of inputs
		createInputs();
		
		// Listeners
		listeners.push(numInputs.newListener([this](int &){
			createInputs();
			updateOutput();
		}));
		
		listeners.push(switchParam.newListener([this](int &){
			updateOutput();
		}));
	}

private:
	void createInputs() {
		// Clear existing inputs
		inputs.clear();
		
		// Remove old parameters from the parameter group
		for(auto& paramName : inputParamNames) {
			if(getParameterGroup().contains(paramName)) {
				removeParameter(paramName);
			}
		}
		inputParamNames.clear();
		
		// Create new inputs based on numInputs
		int numIn = numInputs.get();
		for(int i = 0; i < numIn; i++) {
			string paramName = "Input " + ofToString(i);
			inputParamNames.push_back(paramName);
			
			auto input = make_shared<ofParameter<string>>();
			input->set(paramName, "", "", "");
			inputs.push_back(input);
			
			// Add parameter to the node
			addParameter(*input);
			
			// Add listener for this input
			listeners.push(input->newListener([this](string &){
				updateOutput();
			}));
		}
		
		// Update switch parameter max value
		switchParam.setMax(numIn - 1);
		if(switchParam.get() >= numIn) {
			switchParam.set(numIn - 1);
		}
		
		updateOutput();
	}
	
	void updateOutput() {
		int switchIndex = switchParam.get();
		int numIn = numInputs.get();
		
		// Clamp switch index to valid range
		switchIndex = ofClamp(switchIndex, 0, numIn - 1);
		
		// Set output to the selected input's value
		if(switchIndex < inputs.size() && switchIndex >= 0) {
			output.set(inputs[switchIndex]->get());
		} else {
			output.set("");
		}
	}
	
	// Parameters
	ofParameter<int> numInputs;
	ofParameter<int> switchParam;
	ofParameter<string> output;
	
	// Dynamic inputs
	vector<shared_ptr<ofParameter<string>>> inputs;
	vector<string> inputParamNames;
	
	ofEventListeners listeners;
};

#endif /* stringSwitch_h */
