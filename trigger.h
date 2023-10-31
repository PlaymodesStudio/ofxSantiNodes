//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef TriggerNode_h
#define TriggerNode_h

#include "ofxOceanodeNodeModel.h"
#include <queue>

class trigger : public ofxOceanodeNodeModel {
public:
    trigger() : ofxOceanodeNodeModel("Trigger") {}

    ~trigger() {
            //ofRemoveListener(ofEvents().update, this, &trigger::update);
        }
    
    void setup() override {
        addParameter(inputPh.set("Input ph", {0.0f}, {0.0f}, {1.0f}));
        addParameter(change.set("Change", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(event.set("Event", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(gate.set("Gate", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(chance.set("Chance", 1, 0, 1));
        addOutputParameter(trigOut.set("Trig Out", {0}, {0.0f}, {1.0f}));

        listeners.push(inputPh.newListener([this](vector<float> &f) {
            if(f.size() > lastInputPh.size()){
                lastInputPh.resize(f.size(), 0.0f);
            }
            vector<float> trigOutTemp(f.size(), 0.0f);
            for(int i = 0; i < f.size(); i++){
                if(lastInputPh[i] < 0.5f && f[i] >= 0.5f){
                    float rnd = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                    if(rnd < chance) {
                        trigOutTemp[i] = 0.5f;
                    }
                }
            }
            enqueueOutputValue(trigOutTemp);
            lastInputPh = f;
        }));

        listeners.push(change.newListener([this](vector<float> &vf) {
            if(vf.size() > lastChange.size()){
                lastChange.resize(vf.size(), 0.0f);
            }
            vector<float> trigOutTemp(vf.size(), 0.0f);
            for(int i = 0; i < vf.size(); i++){
                if(vf[i] != lastChange[i]){
                    float rnd = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                    if(rnd < chance) {
                        trigOutTemp[i] = 0.5f;
                    }
                }
            }
            enqueueOutputValue(trigOutTemp);
            lastChange = vf;
        }));

        listeners.push(event.newListener([this](vector<float> &vf) {
            float rnd = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            if(rnd < chance) {
                enqueueOutputValue(vector<float>(vf.size(), 0.5f));
                return;
            }
            enqueueOutputValue(vector<float>(vf.size(), 0.0f));
        }));

        listeners.push(gate.newListener([this](vector<float> &g) {
            if(g.size() > lastGate.size()){
                lastGate.resize(g.size(), 0.0f);
            }
            vector<float> trigOutTemp(g.size(), 0.0f);
            for(int i = 0; i < g.size(); i++){
                if(lastGate[i] <= 0.0f && g[i] > 0.0f){
                    float rnd = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                    if(rnd < chance) {
                        trigOutTemp[i] = 0.5f;
                    }
                }
            }
            enqueueOutputValue(trigOutTemp);
            lastGate = g;
        }));

        // create a listener for your app's update event
        //ofAddListener(ofEvents().update, this, &trigger::update);
    }
    

    void update(ofEventArgs &args) {
        // If we have values to dispatch
        if (!outputQueue.empty()) {
            trigOut = outputQueue.front();
            outputQueue.pop();
        }
    }

    void enqueueOutputValue(vector<float> value) {
        // you enqueue every new output value twice
        outputQueue.push(value);
        outputQueue.push(vector<float>(value.size(), 0.0f));
    }

private:
    ofParameter<vector<float>> inputPh;
    ofParameter<vector<float>> change;
    ofParameter<vector<float>> event;
    ofParameter<vector<float>> gate;
    ofParameter<float> chance;
    ofParameter<vector<float>> trigOut;

    vector<float> lastInputPh = {0.0f};
    vector<float> lastChange = {0.0f};
    vector<float> lastGate = {0.0f};

    ofEventListeners listeners;
    std::queue<vector<float>> outputQueue;
};

#endif /* TriggerNode_h */
