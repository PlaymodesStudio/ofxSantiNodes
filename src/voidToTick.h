#pragma once
#include "ofxOceanodeNodeModel.h"

class voidToTick : public ofxOceanodeNodeModel {
public:
    voidToTick() : ofxOceanodeNodeModel("Void to Tick") {
        description = "Generates a scalar trigger (first 1, then 0) in response to a void input. The sequence occurs within the same update cycle.";
        
        // Add input/output parameters
        addParameter(input.set("Void Input"));
        addOutputParameter(output.set("Tick Output", 0.0f));
        
        // Listen for input trigger
        listener = input.newListener([this]() {
            triggered = true;
        });
    }

    void update(ofEventArgs &args) override {
        if(triggered) {
            output = 1.0f;
            output = 0.0f;
            triggered = false;
        }
    }

private:
    ofParameter<void> input;
    ofParameter<float> output;
    bool triggered = false;
    ofEventListener listener;
};
