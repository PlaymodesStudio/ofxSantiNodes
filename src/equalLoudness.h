#ifndef equalLoudness_h
#define equalLoudness_h

#include "ofxOceanodeNodeModel.h"
#include <memory> // For std::unique_ptr
#include <cmath>

class EqualLoudness : public ofxOceanodeNodeModel {
public:
    EqualLoudness() : ofxOceanodeNodeModel("Equal Loudness") {}

    void setup() override;
    void calculate();

private:
    ofParameter<vector<float>> pitch;
    ofParameter<vector<float>> outputAmplitudes;

    vector<std::unique_ptr<ofEventListener>> listeners;
};

#endif /* equalLoudness_h */
