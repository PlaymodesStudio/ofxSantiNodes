#ifndef texUniForms_h
#define texUniForms_h

#include "ofxOceanodeNodeModel.h"

class texUniForms : public ofxOceanodeNodeModel {
public:
    texUniForms();
    ~texUniForms(){};
    
    void setup() override;
    
private:
    void computeOutput(ofTexture* &in);
    
    ofEventListeners listeners;
    
    ofParameter<int> triggerTextureIndex;
    ofParameter<int> spacing;
    ofParameter<bool> lastSpacing;
    vector<ofParameter<ofTexture*>> inputs;
    vector<ofParameter<vector<float>>> customPositions;
    vector<ofParameter<float>> opacities;
    ofParameter<ofTexture*> output;
    
    ofFbo outputFbo;
};

#endif /* texUniForms_h */
