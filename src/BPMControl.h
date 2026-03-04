#ifndef BPMControl_h
#define BPMControl_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxOsc.h"

class BPMControl : public ofxOceanodeNodeModel {
public:
    BPMControl() : ofxOceanodeNodeModel("BPM Control") {
        lastBPM=0;
        parentContainer = nullptr;
        updatingBpm = false;
        updateBpmInNextCycle = false;

        addParameter(phaseReset.set("PhaseRst"));
        addParameter(bpm.set("BPM", 120.0f, 1.0f, 999.9f));
        addParameter(scopedMode.set("Scoped", false));
        addParameter(port.set("Port", 12345, 1, 65535));
        addParameter(resetOnChg.set("RstPhOnChg",true));
        addParameter(phaseHasReset.set("Trig.Rst"));

        phaseResetListener = phaseReset.newListener([this](){
            applyPhaseReset();
        });

        bpmListener = bpm.newListener([this](float &newBpm){
            applyBPM(newBpm);
            if(lastBPM!=bpm)
            {
                if(resetOnChg) applyPhaseReset();
                lastBPM=bpm;
            }
        });

        portListener = port.newListener([this](int &newPort){
            setupOscSender(newPort);
        });

        setupOscSender(port);
    }
    
    void update(ofEventArgs &a) override{
        if(scopedMode && updateBpmInNextCycle){
            if(parentContainer == nullptr) return;
            updatingBpm = true;
            parentContainer->setBpm(bpm);
            updatingBpm = false;
            updateBpmInNextCycle = false;
        }
    }

    void setContainer(ofxOceanodeContainer* container) override {
        ofxOceanodeNodeModel::setContainer(container);
        parentContainer = container;
        if(scopedMode){
            updatingBpm = true;
            parentContainer->setBpm(bpm);
            updatingBpm = false;
        }
    }

    void resetPhase() override
    {
        phaseHasReset.trigger();
    }
    
    void presetRecallBeforeSettingParameters(ofJson &json) override{
        deserializeParameter(json, scopedMode);
    }
    
    //Listens for setBpm messages from container discard is called by itself
    void setBpm(float _bpm) override{
        if(updatingBpm) return;
        if(scopedMode) updateBpmInNextCycle = true;
    }
    
private:
    void setupOscSender(int newPort){
        sender.setup("localhost", newPort);
    }

    void applyBPM(float newBpm){
        if(scopedMode){
            if(parentContainer == nullptr) return;
            updatingBpm = true;
            parentContainer->setBpm(newBpm);
            updatingBpm = false;
        }else{
            ofxOscMessage m;
            m.setAddress("/bpm");
            m.addFloatArg(newBpm);
            sender.sendMessage(m, false);
        }
    }

    void applyPhaseReset(){
        if(scopedMode){
            if(parentContainer == nullptr) return;
            parentContainer->resetPhase();
        }else{
            ofxOscMessage m;
            m.setAddress("/phaseReset");
            sender.sendMessage(m, false);
        }
    }

    ofParameter<float> bpm;
    ofParameter<bool> scopedMode;
    ofParameter<int> port;
    ofParameter<void> phaseReset;
    ofParameter<void> phaseHasReset;
    ofParameter<bool> resetOnChg;
    ofxOscSender sender;

    ofEventListener bpmListener;
    ofEventListener portListener;
    ofEventListener phaseResetListener;

    float lastBPM;
    ofxOceanodeContainer* parentContainer;
    
    bool updatingBpm;
    bool updateBpmInNextCycle;
};

#endif /* BPMControl_h */
