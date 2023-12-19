#include "ofxOceanodeNodeModel.h"

class ramp : public ofxOceanodeNodeModel {
public:
    ramp() : ofxOceanodeNodeModel("Ramp") {}

    void setup() override {
        addParameter(trigger.set("trigger"));
        addParameter(ms.set("ms", 1000, 1, INT_MAX)); // milliseconds
        addParameter(output.set("output", 0.0f, 0.0f, 1.0f));

        trigger.addListener(this, &ramp::onTrigger);
        
        lastTriggerState = false;
        frameCount = 0;
        rampActive = false;
    }

    void update(ofEventArgs &args) override {
        if(rampActive) {
            int totalFrames = (ms / 1000.0f) * ofGetFrameRate();
            if(frameCount < totalFrames) {
                float rampValue = float(frameCount) / totalFrames;
                output = rampValue;
                frameCount++;
            } else {
                output = 1.0f;
                rampActive = false; // Ramp completed
            }
        }
    }

    void onTrigger() {
        frameCount = 0;
        rampActive = true;
    }

private:
    ofParameter<void> trigger;
    ofParameter<float> ms;
    ofParameter<float> output;

    bool lastTriggerState;
    bool rampActive;
    int frameCount;
};
