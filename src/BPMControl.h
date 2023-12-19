#ifndef BPMControl_h
#define BPMControl_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"

class BPMControl : public ofxOceanodeNodeModel {
public:
    BPMControl() : ofxOceanodeNodeModel("BPM Control") {
        addParameter(bpm.set("BPM", 120.0f, 1.0f, 999.9f)); // Assuming a range from 1 to 999 BPM
        addParameter(port.set("Port", 11575, 1, 65535)); // Range for valid port numbers

        bpmListener = bpm.newListener([this](float &newBpm){
            sendBPM(newBpm);
        });

        portListener = port.newListener([this](int &newPort){
            setupOscSender(newPort);
        });

        setupOscSender(port);
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

    ofParameter<float> bpm;
    ofParameter<int> port;
    ofxOscSender sender;

    ofEventListener bpmListener;
    ofEventListener portListener;
};

#endif /* BPMControl_h */
