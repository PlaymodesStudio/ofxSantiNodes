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


class vectorToColor : public ofxOceanodeNodeModel {
public:
    vectorToColor() : ofxOceanodeNodeModel("Vector To Color"){
        addParameter(R.set("Red", 0.0f, 0.0f, 1.0f));
        addParameter(G.set("Green", 0.0f, 0.0f, 1.0f));
        addParameter(B.set("Blue", 0.0f, 0.0f, 1.0f));
        addParameter(A.set("Alpha", 0.0f, 0.0f, 1.0f));
        addOutputParameter(output.set("Color", ofFloatColor(0.0f, 0.0f, 0.0f, 0.0f)));

        listenerR = R.newListener([this](float &f){
            processColor();
        });
        listenerG = G.newListener([this](float &f){
            processColor();
        });
        listenerB = B.newListener([this](float &f){
            processColor();
        });
        listenerA = A.newListener([this](float &f){
            processColor();
        });

    }

    void processColor()
    {
        ofFloatColor auxColor;
        auxColor.r = R;
        auxColor.g = G;
        auxColor.b = B;
        auxColor.a = A;

        output = auxColor;
    }
private:
    ofParameter<ofFloatColor> output;
    ofParameter<float> R;
    ofParameter<float> G;
    ofParameter<float> B;
    ofParameter<float> A;

    ofEventListener listenerR;
    ofEventListener listenerG;
    ofEventListener listenerB;
    ofEventListener listenerA;
};

#endif /* colorToVector_h */
