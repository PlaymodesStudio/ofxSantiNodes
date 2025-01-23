
#ifndef boolToVoid_h
#define boolToVoid_h

#include "ofxOceanodeNodeModel.h"



class boolToVoid : public ofxOceanodeNodeModel {
public:
    boolToVoid() : ofxOceanodeNodeModel("BoolToVoid") {
        addParameter(triggerReset.set("Reset"));
        addParameter(boolIn.set("Bool In",false));
        addParameter(voidOut.set("Void Out"));
        lastBool = false;
        boolInListener = boolIn.newListener([this](bool &b){
            if(boolIn&&(lastBool!=true))
            {
                voidOut.trigger();
                lastBool=true;
            }
            else if(!boolIn) lastBool=false;
        });
        resetListener = triggerReset.newListener([this]{
            boolIn=false;
        });
    }

private:
    ofParameter<bool> boolIn;
    ofParameter<void> voidOut;
    ofParameter<void> triggerReset;
    ofEventListener boolInListener;
    ofEventListener resetListener;
    bool lastBool;
};

class floatToBool : public ofxOceanodeNodeModel {
public:
    floatToBool() : ofxOceanodeNodeModel("FloatToBool") {
        addParameter(floatIn.set("Float In",0.0,0.0,1.0));
        addParameter(boolOut.set("Bool Out",true));
        lastFloat = 0.0;
        floatInListener = floatIn.newListener([this](float &f){
            if(floatIn==0.0)
            {
                boolOut=false;
            }
            else
            {
                boolOut=true;
            }
        });
    }

private:
    ofParameter<float> floatIn;
    ofParameter<bool> boolOut;
    ofEventListener floatInListener;
    bool lastFloat;
};

class boolToFloat : public ofxOceanodeNodeModel {
public:
    boolToFloat() : ofxOceanodeNodeModel("BoolToFloat") {
        addParameter(boolIn.set("Bool In",false));
        addParameter(floatOut.set("Float Out",0.0,0.0,1.0));
        boolInListener = boolIn.newListener([this](bool &f){
            if(boolIn)
            {
                floatOut=1.0;
            }
            else
            {
                floatOut=0.0;
            }
        });
    }

private:
    ofParameter<bool> boolIn;
    ofParameter<float> floatOut;
    ofEventListener boolInListener;
};

class floatToVoid : public ofxOceanodeNodeModel {
public:
    floatToVoid() : ofxOceanodeNodeModel("FloatToVoid") {
        addParameter(floatIn.set("Float In",0,0,1));
        addParameter(voidOut.set("Void Out"));
        lastFloat = 0.0;
        floatInListener = floatIn.newListener([this](float &f){
            if((floatIn==1.0)&&(lastFloat!=1.0))
            {
                voidOut.trigger();
                lastFloat=1.0;
            }
            else if(floatIn!=1.0) lastFloat=f;
        });
    }

private:
    ofParameter<float> floatIn;
    ofParameter<void> voidOut;
    ofEventListener floatInListener;
    float lastFloat;
};
#endif /* boolToVoid_h */
