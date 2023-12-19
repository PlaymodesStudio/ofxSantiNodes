// counterReset.h

#ifndef counterReset_h
#define counterReset_h

#include "ofxOceanodeNodeModel.h"

class counterReset : public ofxOceanodeNodeModel {
public:
    counterReset() : ofxOceanodeNodeModel("Counter Reset"), counter(0.0f), countStarted(false) {}

    void setup() override {
        addParameter(reset.set("Reset", false));
        addOutputParameter(output.set("Out", 0, 0, FLT_MAX));
        
        resetListener = reset.newListener([this](bool &value) {
            if(value){
                counter = 0.0f;
                reset = false;
                countStarted = true;
            }
        });
    }

    void update(ofEventArgs &a) override {
        if(countStarted) {
            counter += ofGetLastFrameTime();
            output = counter;
        }
    }

private:
    ofParameter<bool> reset;
    ofParameter<float> output;
    ofEventListener resetListener;

    float counter;
    bool countStarted;
};

#endif /* counterReset_h */
