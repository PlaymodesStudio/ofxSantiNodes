#ifndef BPMControl_h
#define BPMControl_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"

class BPMControl : public ofxOceanodeNodeModel {
public:
    BPMControl() : ofxOceanodeNodeModel("BPM Control") {
        lastBPM=0;
        
        addParameter(phaseReset.set("PhaseRst"));
        addParameter(bpm.set("BPM", 120.0f, 1.0f, 999.9f)); // Assuming a range from 1 to 999 BPM
        addParameter(port.set("Port", 12345, 1, 65535)); // Range for valid port numbers
        addParameter(resetOnChg.set("RstPhOnChg",true));
        addParameter(phaseHasReset.set("Trig.Rst"));

        phaseResetListener = phaseReset.newListener([this](){
            sendPhaseReset();
        });
        
        bpmListener = bpm.newListener([this](float &newBpm){
            sendBPM(newBpm);
            if(lastBPM!=bpm)
            {
                if(resetOnChg) sendPhaseReset();
                lastBPM=bpm;
            }
        });

        portListener = port.newListener([this](int &newPort){
            setupOscSender(newPort);
        });

        setupOscSender(port);
    }

    void resetPhase() override
    {
        //cout << "Reset Phaseeeee" << endl;
        phaseHasReset.trigger();
    }
private:
    void setupOscSender(int newPort){
        sender.setup("localhost", newPort);
    }

    void sendBPM(float newBpm){
        ofxOscMessage m;
        m.setAddress("/bpm");
        m.addFloatArg(newBpm);
        sender.sendMessage(m, false);
    }
    void sendPhaseReset(){
        ofxOscMessage m;
        m.setAddress("/phaseReset");
        sender.sendMessage(m, false);
    }

    ofParameter<float> bpm;
    ofParameter<int> port;
    ofParameter<void> phaseReset;
    ofParameter<void> phaseHasReset;
    ofParameter<bool> resetOnChg;
    ofxOscSender sender;

    ofEventListener bpmListener;
    ofEventListener portListener;
    ofEventListener phaseResetListener;
    
    float lastBPM;
};

#endif /* BPMControl_h */
