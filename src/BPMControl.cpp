#include "BPMControl.h"
#include "ofxOceanodeContainer.h"

void BPMControl::update(ofEventArgs &a) {
    if(scopedMode && updateBpmInNextCycle){
        if(parentContainer == nullptr) return;
        updatingBpm = true;
        parentContainer->setBpm(bpm);
        updatingBpm = false;
        updateBpmInNextCycle = false;
    }
}

void BPMControl::setContainer(ofxOceanodeContainer* container) {
    ofxOceanodeNodeModel::setContainer(container);
    parentContainer = container;
    if(scopedMode){
        updatingBpm = true;
        parentContainer->setBpm(bpm);
        updatingBpm = false;
    }
}

void BPMControl::applyBPM(float newBpm) {
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

void BPMControl::applyPhaseReset() {
    if(scopedMode){
        if(parentContainer == nullptr) return;
        parentContainer->resetPhase();
    }else{
        ofxOscMessage m;
        m.setAddress("/phaseReset");
        sender.sendMessage(m, false);
    }
}
