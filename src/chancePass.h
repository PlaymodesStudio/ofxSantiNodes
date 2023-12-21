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
               ensureVectorSize(vf.size());
               vector<float> tempOut(output->size());
               for(int i = 0; i < vf.size(); i++){
                   tempOut[i] = (dist(mt[i]) < getProbability(i)) ? vf[i] : output->at(i);
               }
               output = tempOut;
           }));

        listeners.push(gateIn.newListener([this](vector<float> &vf){
            ensureVectorSize(vf.size());
            vector<float> tempOut(output->size());
            for(int i = 0; i < vf.size(); i++){
                if(vf[i] > 0 && dist(mt[i]) < getProbability(i)){
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
    
    void ensureVectorSize(size_t newSize) {
        if (output->size() != newSize) {
            vector<float> newOutput(newSize, 0.0f); // Create a new vector with the desired size and default value.
            output.set(newOutput); // Set the new vector to the output parameter.
            mt.resize(newSize); // Resize the mt vector.
            setSeed(); // Reset the seed.
        }
    }

    
    float getProbability(size_t index) {
        return probability->size() == output->size() ? probability->at(index) : probability->at(0);
    }
    
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

