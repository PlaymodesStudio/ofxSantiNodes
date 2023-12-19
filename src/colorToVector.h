#ifndef colorToVector_h
#define colorToVector_h

#include "ofxOceanodeNodeModel.h"

class colorToVector : public ofxOceanodeNodeModel {
public:
    colorToVector() : ofxOceanodeNodeModel("Color To Vector"){
        addParameter(input.set("Color", ofFloatColor(0.0f, 0.0f, 0.0f, 0.0f)));
        addOutputParameter(output.set("Output", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}));

        listener = input.newListener([this](ofFloatColor &c){
            vector<float> colorVector;
            colorVector.push_back(c.r);
            colorVector.push_back(c.g);
            colorVector.push_back(c.b);
            output = colorVector;
        });
    }

private:
    ofParameter<ofFloatColor> input;
    ofParameter<vector<float>> output;

    ofEventListener listener;
};

#endif /* colorToVector_h */
