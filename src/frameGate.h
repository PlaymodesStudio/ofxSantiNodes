// frameGate.h

#ifndef frameGate_h
#define frameGate_h

#include "ofxOceanodeNodeModel.h"

class frameGate : public ofxOceanodeNodeModel {
public:
    frameGate() : ofxOceanodeNodeModel("Frame Gate") {}

    void setup() override {
        addParameter(input.set("Input", {0.0f}, {FLT_MIN}, {FLT_MAX}));
        addParameter(chance.set("Chance", 0.5, 0.0, 1.0));
        addOutputParameter(output.set("Output", {0.0f}, {FLT_MIN}, {FLT_MAX}));

        shouldOutputZero.resize(1, false); // Initial size of 1

        listeners.push(input.newListener([this](vector<float> &vf){
            vector<float> outValues(vf.size(), 0.0f);

            if(vf.size() != shouldOutputZero.size()) {
                shouldOutputZero.resize(vf.size(), false);
            }

            for(int i = 0; i < vf.size(); i++) {
                if(shouldOutputZero[i]) {
                    outValues[i] = 0;
                    shouldOutputZero[i] = false;
                }
                else {
                    float rnd = ofRandom(1.0);
                    if(rnd < chance) {
                        outValues[i] = vf[i];
                        shouldOutputZero[i] = true; // Mark this index to output zero in the next frame
                    }
                }
            }
            output = outValues;
        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<float> chance;
    ofParameter<vector<float>> output;

    vector<bool> shouldOutputZero;

    ofEventListeners listeners;
};

#endif /* FrameGate_h */
