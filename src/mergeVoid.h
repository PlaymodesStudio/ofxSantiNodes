#ifndef mergeVoid_h
#define mergeVoid_h

#include "ofxOceanodeNodeModel.h"

class mergeVoid : public ofxOceanodeNodeModel {
public:
    mergeVoid() : ofxOceanodeNodeModel("Merge Void") {
        description = "Merges void signals from various inputs into one output";

        // Output void parameter
        addOutputParameter(voidOut.set("Void Out"));

        // Setup void input parameters and listen to them
        for (int i = 0; i < 4; ++i) {
            ofParameter<void> newInput;
            newInput.setName("In " + ofToString(i + 1));
            addParameter(newInput);
            inputs.push_back(newInput);

            // Directly attach listeners to input parameters
            listeners[i] = newInput.newListener([this]() {
                this->voidOut.trigger(); // Trigger the output when any input is activated
            });
        }
    }

private:
    ofParameter<void> voidOut; // Void output parameter
    std::vector<ofParameter<void>> inputs; // Store input parameters
    std::array<ofEventListener, 4> listeners; // Use an array for listeners
};

#endif /* mergeVoid_h */
