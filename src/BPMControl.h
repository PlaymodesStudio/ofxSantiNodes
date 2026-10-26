#ifndef BPMControl_h
#define BPMControl_h

#include "ofxOceanodeNodeModel.h"
class ofxOceanodeContainer;
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
        addOutputParameter(phaseHasReset.set("Trig.Rst"));

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
    
    void update(ofEventArgs &a) override;

    void setContainer(ofxOceanodeContainer* container) override;

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

    void applyBPM(float newBpm);

    void applyPhaseReset();

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
