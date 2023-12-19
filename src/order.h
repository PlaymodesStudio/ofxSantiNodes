#ifndef ORDER_H
#define ORDER_H

#include "ofxOceanodeNodeModel.h"

class order : public ofxOceanodeNodeModel {
public:
    order() : ofxOceanodeNodeModel("Order") {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(frameMode.set("Frame", false));

        // Initialize with 4 outputs
        for (int i = 0; i < 4; i++) {
            ofParameter<vector<float>> newOutput;
            newOutput.set("Output " + ofToString(i + 1), {0.0f}, {-FLT_MAX}, {FLT_MAX});
            addOutputParameter(newOutput);
            outputs.push_back(newOutput);
            deque<vector<float>> newStore;
            stores.push_back(newStore);
        }

        listener = input.newListener([this](vector<float> &in) {
            handleInput(in);
        });
    }

    void update(ofEventArgs &a) {
        if (frameMode) {
            for (int i = 0; i < outputs.size(); i++) {
                if (!stores[i].empty()) {
                    outputs[i] = stores[i].front();
                    stores[i].pop_front();
                }
            }
        }
    }

private:
    void handleInput(const vector<float> &in) {
        if (frameMode) {
            for (int i = 0; i < outputs.size(); i++) {
                stores[i].push_back(in);
            }
        } else {
            for (auto &output : outputs) {
                output = in;
            }
        }
    }

    ofParameter<vector<float>> input;
    ofParameter<bool> frameMode;
    vector<ofParameter<vector<float>>> outputs;
    ofEventListener listener;
    vector<deque<vector<float>>> stores;
};


#endif // ORDER_H
