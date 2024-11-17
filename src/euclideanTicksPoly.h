// euclideanTicksPoly.h
#ifndef euclideanTicksPoly_h
#define euclideanTicksPoly_h

#include "ofxOceanodeNodeModel.h"
#include <vector>

class euclideanTicksPoly : public ofxOceanodeNodeModel {
public:
    euclideanTicksPoly() : ofxOceanodeNodeModel("Euclidean Ticks Poly") {}
    
    void setup();
    void update(ofEventArgs &args);  // Add the update method
    void calculate();
    void reset();
    
private:
    // Input Parameters
    ofParameter<std::vector<int>> inCount;
    ofParameter<std::vector<int>> length;
    ofParameter<std::vector<int>> onsets;
    ofParameter<std::vector<int>> offset;
    ofParameter<bool> retrigger;
    
    // Output Parameters
    ofParameter<std::vector<float>> gatesOut;
    ofParameter<std::vector<int>> outCount;
    ofParameter<void> tick;
    
    // Control Parameters
    ofParameter<void> resetButton;
    ofParameter<void> resetNext;
    
    // Event Listeners
    ofEventListener listener;
    ofEventListener resetButtonListener;
    ofEventListener resetNextListener;
    ofEventListener updateListener;  // Listener for the update method
    
    // Internal State Variables
    std::vector<int> outCountVector;
    std::vector<int> prevInCount;
    std::vector<float> previousGatesOut;
    std::vector<bool> insertZeroNextFrame;  // Flag for inserting zero in the next frame
    bool shouldResetNext = false;
};

#endif /* euclideanTicksPoly_h */
