#ifndef prepend_h
#define prepend_h

#include "ofxOceanodeNodeModel.h"

class prepend : public ofxOceanodeNodeModel {
public:
	prepend() : ofxOceanodeNodeModel("Prepend") {}
	
	void setup() override {
		description = "Prepends a specified string to the input string. Optional space can be added between them";
		
		addParameter(inputString.set("Input", ""));
		addParameter(prependString.set("Prepend", ""));
		addParameter(addSpace.set("Add Space", false));
		addOutputParameter(outputString.set("Output", ""));
		
		listener = inputString.newListener([this](string &){
			process();
		});
		
		prependListener = prependString.newListener([this](string &){
			process();
		});

		spaceListener = addSpace.newListener([this](bool &){
			process();
		});
	}
	
private:
	void process() {
		string prefix = prependString.get();
		string input = inputString.get();
		
		// Handle empty strings
		if(prefix.empty()) {
			outputString = input;
			return;
		}
		if(input.empty()) {
			outputString = prefix;
			return;
		}
		
		// Combine strings with optional space
		if(addSpace) {
			outputString = prefix + " " + input;
		} else {
			outputString = prefix + input;
		}
	}
	
	ofParameter<string> inputString;
	ofParameter<string> prependString;
	ofParameter<string> outputString;
	ofParameter<bool> addSpace;
	ofEventListener listener;
	ofEventListener prependListener;
	ofEventListener spaceListener;
};

#endif /* prepend_h */
