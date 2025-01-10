#pragma once
#include "ofxOceanodeNodeModel.h"

class voidToTick : public ofxOceanodeNodeModel {
public:
    voidToTick() : ofxOceanodeNodeModel("Void to Tick") {
        description = "Generates a scalar trigger (0 → 1 → 0) in response to a void input over consecutive frames.";
        
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
            // On the frame we receive the trigger, set output to 1
            output = 1.0f;
            triggered = false;
            latch = true; // Latch means next frame we'll reset it back
        } else if(latch) {
            // On the following frame, set output back to 0
            output = 0.0f;
            latch = false;
        }
        // If neither triggered nor latch, output remains whatever it was (usually 0).
    }

private:
    ofParameter<void> input;
    ofParameter<float> output;
    bool triggered = false;
    bool latch = false;  // Used to remember to reset on next frame
    ofEventListener listener;
};
