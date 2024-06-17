#ifndef harmonicSeries_h
#define harmonicSeries_h

#include "ofxOceanodeNodeModel.h"
#include <memory> // For std::unique_ptr

class harmonicSeries : public ofxOceanodeNodeModel {
public:
    harmonicSeries() : ofxOceanodeNodeModel("Harmonic Series") {}

    void setup() override;
    void calculate();
    void calculateDetuneFactors();

private:
    ofParameter<vector<float>> pitch;
    ofParameter<vector<float>> ampIn;
    ofParameter<int> partialsNum;
    ofParameter<int> harmonicShape;
    ofParameter<vector<float>> output;
    ofParameter<vector<float>> outputPitch;
    ofParameter<vector<float>> amplitudes;
    ofParameter<vector<float>> hpCutoff;
    ofParameter<vector<float>> lpCutoff;
    ofParameter<float> detuneAmount;
    vector<float> detuneFactors;
    ofParameter<float>oddHarmonicAmp;
    ofParameter<float>evenHarmonicAmp;
    ofParameter<float>harmonicStretch;
    ofParameter<vector<float>> sortedFreq;
    ofParameter<vector<float>> sortedPitch;
    ofParameter<vector<float>> sortedAmp;

    vector<std::unique_ptr<ofEventListener>> listeners;
};

#endif /* harmonicSeries_h */
