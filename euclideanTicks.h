//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023
#ifndef euclideanTicks_h
#define euclideanTicks_h

#include "ofxOceanodeNodeModel.h"

class euclideanTicks : public ofxOceanodeNodeModel {
public:
    euclideanTicks() : ofxOceanodeNodeModel("Euclidean Ticks"), totalOnsets(0), prevInCount(-1), pulseContIndex(0) {}

    void setup() override {
        addParameter(inCount.set("In Count", 0, 0, INT_MAX));
        addParameter(length.set("Length", 1, 1, INT_MAX));
        addParameter(onsets.set("Onsets", 1, 0, INT_MAX));
        addParameter(offset.set("Offset", 0, 0, INT_MAX));
        addOutputParameter(pulse.set("Pulse", {0.0f}, {0.0f}, {1.0f}));
        addOutputParameter(outCount.set("Out Count", 0, 0, INT_MAX));
        addOutputParameter(pulseCont.set("PulseCont", {0.0f}, {0.0f}, {1.0f}));

        listener = inCount.newListener([this](int &value) {
            calculate();
        });
    }

    void calculate();

private:
    ofParameter<int> inCount;
    ofParameter<int> length;
    ofParameter<int> onsets;
    ofParameter<int> offset;
    ofParameter<vector<float>> pulse;
    ofParameter<int> outCount;
    ofParameter<vector<float>> pulseCont;

    ofEventListener listener;

    int totalOnsets;
    int prevInCount;
    int pulseContIndex;
};

#endif /* EuclideanTicks_h */
