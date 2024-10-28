#pragma once
#include "ofxOceanodeNodeModel.h"

class voidToTick : public ofxOceanodeNodeModel {
public:
    voidToTick() : ofxOceanodeNodeModel("void to tick") {
        description = "Generates a scalar trigger (first 0, then 1) in response to a void input. The sequence occurs within the same update cycle.";
        
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
            // First output 0, then immediately output 1 within the same update cycle
            output = 0.0f;
            output = 1.0f;
            triggered = false;  // Reset the trigger after processing
        }
    }

private:
    ofParameter<void> input;
    ofParameter<float> output;
    bool triggered = false;
    ofEventListener listener;
};
