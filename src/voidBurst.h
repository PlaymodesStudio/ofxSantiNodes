#pragma once

#include "ofxOceanodeNodeModel.h"
#include <deque>

class voidBurst : public ofxOceanodeNodeModel {
public:
    voidBurst() : ofxOceanodeNodeModel("Void Burst") {
        description = "Expands each incoming void trigger into a burst of void outputs spread across frames. Frame Gap sets how many empty frames sit between each trigger; 0 fires the whole burst in the current frame.";

        addParameter(triggerIn.set("Trigger"));
        addParameter(numVoids.set("Num", 4, 1, INT_MAX));
        addParameter(frameGap.set("Frame Gap", 2, 0, INT_MAX));
        addOutputParameter(voidOut.set("Void Out"));
        addOutputParameter(endOut.set("End"));

        listeners.push(triggerIn.newListener([this]() {
            startBurst();
        }));
    }

    void update(ofEventArgs &) override {
        const uint64_t currentFrame = ofGetFrameNum();

        for(auto it = activeBursts.begin(); it != activeBursts.end();) {
            if(currentFrame < it->nextTriggerFrame) {
                ++it;
                continue;
            }

            if(it->endPending) {
                endOut.trigger();
                it = activeBursts.erase(it);
            } else {
                voidOut.trigger();
                it->remainingTriggers--;

                if(it->remainingTriggers <= 0) {
                    it->endPending = true;
                }

                it->nextTriggerFrame += static_cast<uint64_t>(it->frameGap + 1);
                ++it;
            }
        }
    }

private:
    struct ScheduledBurst {
        int remainingTriggers;
        int frameGap;
        uint64_t nextTriggerFrame;
        bool endPending;
    };

    void startBurst() {
        const int totalTriggers = numVoids.get();
        if(totalTriggers <= 0) return;

        voidOut.trigger();

        if(totalTriggers == 1) return;

        const int gap = frameGap.get();
        if(gap == 0) {
            for(int i = 1; i < totalTriggers; ++i) {
                voidOut.trigger();
            }
            endOut.trigger();
            return;
        }

        activeBursts.push_back({
            totalTriggers - 1,
            gap,
            ofGetFrameNum() + static_cast<uint64_t>(gap + 1),
            false
        });
    }

    ofParameter<void> triggerIn;
    ofParameter<int> numVoids;
    ofParameter<int> frameGap;
    ofParameter<void> voidOut;
    ofParameter<void> endOut;

    std::deque<ScheduledBurst> activeBursts;
    ofEventListeners listeners;
};
