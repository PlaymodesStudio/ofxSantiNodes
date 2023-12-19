#ifndef chancePass_h
#define chancePass_h

#include "ofxOceanodeNodeModel.h"
#include <random>

class chancePass : public ofxOceanodeNodeModel {
public:
    chancePass() : ofxOceanodeNodeModel("Chance Pass"){};
    
    void setup(){
        addParameter(numberIn.set("Number", {0.0f}, {FLT_MIN}, {FLT_MAX}));
        addParameter(gateIn.set("Gate", {0.0f}, {0.0f}, {1.0f}));
        addParameter(seed.set("Seed", {0}, {(INT_MIN+1)/2}, {(INT_MAX-1)/2}));
        addParameter(probability.set("Prob", {0.5}, {0}, {1}));
        addOutputParameter(output.set("Output", {0.0f}, {0.0f}, {FLT_MAX}));
        
        dist = std::uniform_real_distribution<float>(0.0, 1.0);
        mt.resize(1);
        setSeed();
        
        listeners.push(numberIn.newListener([this](vector<float> &vf){
            vector<float> tempOut(output);
            if(tempOut.size() != vf.size()){
                // Resize everything
                tempOut.resize(vf.size());
                setSeed();
            }
            for(int i = 0; i < vf.size(); i++){
                if(dist(mt[i]) < (probability->size() == vf.size() ? probability->at(i) : probability->at(0))){
                    tempOut[i] = vf[i];
                }
            }
            output = tempOut;
        }));

        listeners.push(gateIn.newListener([this](vector<float> &vf){
            vector<float> tempOut(output);
            if(tempOut.size() != vf.size()){
                // Resize everything
                tempOut.resize(vf.size());
                setSeed();
            }
            for(int i = 0; i < vf.size(); i++){
                if(vf[i] > 0 && dist(mt[i]) < (probability->size() == vf.size() ? probability->at(i) : probability->at(0))){
                    tempOut[i] = vf[i];
                }else{
                    tempOut[i] = 0;
                }
            }
            output = tempOut;
        }));
        
        listeners.push(seed.newListener([this](vector<int> &i){
            setSeed();
        }));
    }
    
private:
    
    void setSeed(){
        for(int i = 0; i < mt.size(); i++){
            if(seed->size() == mt.size()){
                mt[i].seed(seed->at(i));
            }
            else{
                if(seed->at(0) == 0){
                    std::random_device rd;
                    mt[i].seed(rd());
                }else{
                    mt[i].seed(seed->at(0) + i);
                }
            }
        }
    }
    
    ofEventListeners listeners;
    
    ofParameter<vector<float>> numberIn;
    ofParameter<vector<float>> gateIn;
    ofParameter<vector<int>> seed;
    ofParameter<vector<float>> probability;
    ofParameter<vector<float>> output;
    
    vector<std::mt19937> mt;
    std::uniform_real_distribution<float> dist;
};

#endif /* chancePass_h */

