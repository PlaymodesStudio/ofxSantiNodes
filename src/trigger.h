//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef TriggerNode_h
#define TriggerNode_h

#include "ofxOceanodeNodeModel.h"
// Transport mode needs ofxOceanode's global transport (feature-globalTransport branch).
// Without it the node compiles exactly as before.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
#include "ofxOceanodeDeterministicRandom.h"
#endif
#include <queue>
#include <cfloat>
#include <climits>

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
                    if(passes(i)) {
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
                    if(passes(i)) {
                        trigOutTemp[i] = 0.5f;
                    }
                }
            }
            enqueueOutputValue(trigOutTemp);
            lastChange = vf;
        }));

        listeners.push(event.newListener([this](vector<float> &vf) {
            if(passes(0)) {
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
                    if(passes(i)) {
                        trigOutTemp[i] = 0.5f;
                    }
                }
            }
            enqueueOutputValue(trigOutTemp);
            lastGate = g;
        }));

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
        // ---- Sync To Transport ----
        // The chance roll becomes a pure function of (Seed, Step, lane): one decision per
        // lane per step, so scrubbing the timeline gives the same pass/fail pattern as playback.
        sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
        addInspectorParameter(syncToTransport_Param.set("Sync To Transport", false));
        listeners.push(syncToTransport_Param.newListener([this](bool &b){
            setTransportInputsVisible(b);
        }));
#endif

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

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
    void loadBeforeConnections(ofJson &json) override {
        // Restore the mode before connections so saved "Step"/"Seed" connections find their inputs.
        deserializeParameter(json, syncToTransport_Param);
    }
#endif

private:
    bool passes(int lane) {
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
        if(syncToTransport_Param) return transportPasses(lane);
#endif
        float rnd = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        return rnd < chance;
    }

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
    bool transportPasses(int lane) {
        const auto &steps = step_Param.get();
        const float stepValue = steps.empty() ? 0.0f : (lane < (int)steps.size() ? steps[lane] : steps[0]);
        const int64_t step = ofxOceanodeDeterministicRandom::stepFromFloat(stepValue);
        const uint64_t key = ofxOceanodeDeterministicRandom::seedKey(seed_Param.get(), sessionSalt);
        return ofxOceanodeDeterministicRandom::uniform(key, step, static_cast<uint64_t>(lane)) < chance;
    }

    void setTransportInputsVisible(bool visible) {
        const bool present = getParameterGroup().contains("Step");
        if(visible && !present) {
            addParameter(step_Param.set("Step", {0}, {0}, {FLT_MAX}));
            addParameter(seed_Param.set("Seed", 0, INT_MIN, INT_MAX));
        } else if(!visible && present) {
            getOceanodeParameter(step_Param).removeAllConnections();
            getOceanodeParameter(seed_Param).removeAllConnections();
            removeParameter("Step");
            removeParameter("Seed");
        }
    }

    ofParameter<bool> syncToTransport_Param;
    ofParameter<vector<float>> step_Param; // only present in Sync To Transport mode
    ofParameter<int> seed_Param;           // only present in Sync To Transport mode
    uint64_t sessionSalt = 0;
#endif

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
