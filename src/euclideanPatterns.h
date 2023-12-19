//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef euclideanPatterns_h
#define euclideanPatterns_h

#include "ofxOceanodeNodeModel.h"

class euclideanPatterns : public ofxOceanodeNodeModel {
public:
    euclideanPatterns() : ofxOceanodeNodeModel("Euclidean Patterns") {}

    void setup() override {
        addParameter(length.set("Length", 1, 1, INT_MAX));
        addParameter(onsets.set("Onsets", 1, 0, INT_MAX));
        addParameter(offset.set("Offset", 0, 0, INT_MAX));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {1.0f}));

        lengthListener = length.newListener([this](int &value) {
            calculate();
        });

        onsetsListener = onsets.newListener([this](int &value) {
            calculate();
        });

        offsetListener = offset.newListener([this](int &value) {
            calculate();
        });
    }

    void calculate();

private:
    ofParameter<int> length;
    ofParameter<int> onsets;
    ofParameter<int> offset;
    ofParameter<vector<float>> output;

    ofEventListener lengthListener;
    ofEventListener onsetsListener;
    ofEventListener offsetListener;
};

#endif /* euclideanPatterns_h */
