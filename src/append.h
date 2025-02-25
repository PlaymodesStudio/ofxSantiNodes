#ifndef append_h
#define append_h

#include "ofxOceanodeNodeModel.h"

class append : public ofxOceanodeNodeModel {
public:
	append() : ofxOceanodeNodeModel("Append") {}
	
	void setup() override {
		description = "Appends a specified string to the input string. Optional space can be added between them";
		
		addParameter(inputString.set("Input", ""));
		addParameter(appendString.set("Append", ""));
		addParameter(addSpace.set("Add Space", false));
		addOutputParameter(outputString.set("Output", ""));
		
		listener = inputString.newListener([this](string &){
			process();
		});
		
		appendListener = appendString.newListener([this](string &){
			process();
		});

		spaceListener = addSpace.newListener([this](bool &){
			process();
		});
	}
	
private:
	void process() {
		string input = inputString.get();
		string suffix = appendString.get();
		
		// Handle empty strings
		if(suffix.empty()) {
			outputString = input;
			return;
		}
		if(input.empty()) {
			outputString = suffix;
			return;
		}
		
		// Combine strings with optional space
		if(addSpace) {
			outputString = input + " " + suffix;
		} else {
			outputString = input + suffix;
		}
	}
	
	ofParameter<string> inputString;
	ofParameter<string> appendString;
	ofParameter<string> outputString;
	ofParameter<bool> addSpace;
	ofEventListener listener;
	ofEventListener appendListener;
	ofEventListener spaceListener;
};

#endif /* append_h */
