#ifndef stringComparator_h
#define stringComparator_h

#include "ofxOceanodeNodeModel.h"

class stringComparator : public ofxOceanodeNodeModel {
public:
	stringComparator() : ofxOceanodeNodeModel("String Comparator") {}

	void setup() override {
		description = "Compares two string inputs. Outputs 1 when strings are equal, 0 when different.";
		
		addParameter(stringA.set("String A", ""));
		addParameter(stringB.set("String B", ""));
		addOutputParameter(result.set("Result", 0, 0, 1));
		
		// Add listeners to trigger comparison when either input changes
		listeners.push(stringA.newListener([this](string &){
			compareStrings();
		}));
		
		listeners.push(stringB.newListener([this](string &){
			compareStrings();
		}));
	}

private:
	void compareStrings() {
		// Compare the two strings and set result to 1 if equal, 0 if different
		result = (stringA.get() == stringB.get()) ? 1 : 0;
	}

	ofParameter<string> stringA;
	ofParameter<string> stringB;
	ofParameter<int> result;
	ofEventListeners listeners;
};

#endif /* stringComparator_h */
