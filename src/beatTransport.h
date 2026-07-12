#pragma once
#include "santiNodesTransportCompat.h"
#include "ofxOceanodeNodeModel.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>

class beatTransport : public ofxOceanodeNodeModel {
public:
    beatTransport() : ofxOceanodeNodeModel("Beat Transport") {
        description = "Outputs Oceanode's shared transport state for testing and transport-driven patching.";
    }

    void setup() override {
        addSeparator("Transport", ofColor(200));
        addOutputParameter(beatOut.set("Beat Transport", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(prevBeatOut.set("Prev Beat", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(deltaBeatOut.set("Delta Beat", 0.0f, -FLT_MAX, FLT_MAX));
        addOutputParameter(beatPhaseOut.set("Beat Phase", 0.0f, 0.0f, 1.0f));
        addOutputParameter(bpmOut.set("BPM", 120.0f, 0.0f, 400.0f));
        addOutputParameter(playingOut.set("Playing", 1, 0, 1));
        addOutputParameter(generationOut.set("Generation", 0, 0, INT_MAX));
        addOutputParameter(driverModeOut.set("Driver Mode", 0, 0, 2));
        addOutputParameter(resetOut.set("Reset"));
        addParameter(resetTransport.set("Reset Transport"));

        listeners.push(resetTransport.newListener([this]() {
            auto transport = getTransport();
            if(transport != nullptr) {
                transport->seekToBeat(0.0);
            }
        }));
    }

    void update(ofEventArgs &) override {
        const auto frameState = getFrameTransportState();
        const auto &previous = frameState.previous;
        const auto &current = frameState.current;

        beatOut = static_cast<float>(current.beatPosition);
        prevBeatOut = static_cast<float>(previous.beatPosition);
        deltaBeatOut = static_cast<float>(current.beatPosition - previous.beatPosition);
        beatPhaseOut = static_cast<float>(ofxOceanodeTransportUtils::wrapPhase(current.beatPosition, 1.0));
        bpmOut = current.bpm;
        playingOut = current.isPlaying ? 1 : 0;
        generationOut = static_cast<int>(std::min<uint64_t>(current.generation, static_cast<uint64_t>(INT_MAX)));
        driverModeOut = static_cast<int>(current.driverMode);

        if(hasSeenGeneration) {
            if(current.generation != lastGeneration) {
                resetOut.trigger();
            }
        } else {
            hasSeenGeneration = true;
        }

        lastGeneration = current.generation;
    }

private:
    ofParameter<float> beatOut;
    ofParameter<float> prevBeatOut;
    ofParameter<float> deltaBeatOut;
    ofParameter<float> beatPhaseOut;
    ofParameter<float> bpmOut;
    ofParameter<int> playingOut;
    ofParameter<int> generationOut;
    ofParameter<int> driverModeOut;
    ofParameter<void> resetOut;
    ofParameter<void> resetTransport;
    ofEventListeners listeners;

    bool hasSeenGeneration = false;
    uint64_t lastGeneration = 0;
};

#endif
