// euclideanTicksPoly.h
#ifndef euclideanTicksPoly_h
#define euclideanTicksPoly_h

#include "ofxOceanodeNodeModel.h"
#include <vector>

class euclideanTicksPoly : public ofxOceanodeNodeModel {
public:
    euclideanTicksPoly() : ofxOceanodeNodeModel("Euclidean Ticks Poly") {}

    void setup();
    void calculate();
    void reset();

private:
    ofParameter<std::vector<int>> inCount;
    ofParameter<std::vector<int>> length;
    ofParameter<std::vector<int>> onsets;
    ofParameter<std::vector<int>> offset;
    ofParameter<std::vector<float>> gatesOut;
    ofParameter<std::vector<float>> gates2Out;  // New gates2Out parameter
    std::vector<float> prevGatesOut;            // To track the previous state of gatesOut
    
    ofParameter<std::vector<int>> outCount;
    ofParameter<void> resetButton;
    ofParameter<void> resetNext;  // New ResetNext parameter
    ofParameter<void> tick;

    ofEventListener listener;
    ofEventListener resetButtonListener;
    ofEventListener resetNextListener;  // Listener for the resetNext

    std::vector<int> outCountVector;
    std::vector<int> prevInCount;
    bool shouldResetNext = false;  // To track if a reset is needed on the next matching number
};

#endif /* euclideanTicksPoly_h */
