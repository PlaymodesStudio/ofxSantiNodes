#pragma once
#include "ofxOceanodeNodeModel.h"

class eventGate : public ofxOceanodeNodeModel {
public:
    eventGate()
    : ofxOceanodeNodeModel("Event Gate")
    , needsGate(false)
    , latch(false)
    {
        // You can add a description if desired
        description = "Generates a one-frame vector trigger (1 → 0) when Input changes.";

        // Add your parameters
        addParameter(input.set("Input", {0.0f}, {0.0f}, {FLT_MAX}));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {1.0f}));

        // Listen for any change in the input vector
        listener = input.newListener([this](vector<float> & vf){
            needsGate = true;    // Mark that we have a new 'event'
        });
    }

    // Override the correct update signature
    void update(ofEventArgs &args) override {
        // If we received a trigger and haven't latched this frame yet
        if(needsGate && !latch){
            // Output a vector of all ones
            output = vector<float>(input->size(), 1.0f);

            // Reset our flags accordingly
            needsGate = false;
            latch = true;
        }
        // If we were latched on the previous frame, reset to zeros now
        else if(latch){
            output = vector<float>(input->size(), 0.0f);
            latch = false;
        }
        // Otherwise, do nothing and keep output as it is
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> output;

    bool needsGate;
    bool latch;

    // Keep a local ofEventListener so it doesn’t go out of scope
    ofEventListener listener;
};
