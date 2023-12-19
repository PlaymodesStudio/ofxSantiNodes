// voidToGate.h
#ifndef voidToGate_h
#define voidToGate_h

#include "ofxOceanodeNodeModel.h"

class voidToGate : public ofxOceanodeNodeModel {
public:
    voidToGate() : ofxOceanodeNodeModel("Void to Gate") {
        addParameter(tickIn.set("Tick In"));
        addOutputParameter(gateOut.set("Gate", 0.0f, 0.0f, 1.0f));

        tickInListener = tickIn.newListener([this](){
            gateOut = 1.0f;
            shouldReset = true;
        });
    }

    void update(){
        if(shouldReset){
            gateOut = 0.0f;
            shouldReset = false;
        }
    }

private:
    ofParameter<void> tickIn;
    ofParameter<float> gateOut;
    bool shouldReset = false;
    ofEventListener tickInListener;
};

#endif /* voidToGate_h */
