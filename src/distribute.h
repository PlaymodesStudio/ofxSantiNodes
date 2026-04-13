#ifndef distribute_h
#define distribute_h

#include "ofxOceanodeNodeModel.h"

class distribute : public ofxOceanodeNodeModel {
public:
    distribute() : ofxOceanodeNodeModel("Distribute") {
        description = "Routes each element of the incoming data to one of N outputs based on the 'Route To' setting per element. Optionally keeps the last value on the unoccupied outputs. The 'Event' toggle enables processing only on new unique inputs.";

        addInspectorParameter(numOutputs.set("Num Outputs", 2, 1, 16));
    }

    void setup() override {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(routeTo.set("Route To", {1}, {1}, {numOutputs.get()}));
        addParameter(keep.set("Keep", false));
        addParameter(event.set("Event", false));

        int n = numOutputs.get();
        outputs.resize(n);
        lastOutputs.resize(n);
        for (int i = 0; i < n; i++) {
            outputs[i].set("Output " + ofToString(i + 1), {0.0f}, {-FLT_MAX}, {FLT_MAX});
            addOutputParameter(outputs[i]);
        }

        listeners.push(numOutputs.newListener([this](int &) {
            updateNumOutputs();
        }));

        listeners.push(input.newListener([this](vector<float> &in) {
            if (!event.get() || in != lastInputValue) {
                updateOutputs();
                lastInputValue = in;
            }
        }));

        // Don't automatically update outputs when routeTo changes
        // Only update routing configuration, outputs will update on next input
        listeners.push(routeTo.newListener([this](vector<int> &) {
            // Just update the routing configuration, don't trigger output
        }));

        // Don't automatically update outputs when keep changes
        // Only update on next input
        listeners.push(keep.newListener([this](bool &) {
            // Just update the keep configuration, don't trigger output
        }));
    }

private:
    void updateNumOutputs() {
        int oldSize = (int)outputs.size();
        int newSize = numOutputs.get();

        if (newSize < oldSize) {
            for (int i = oldSize - 1; i >= newSize; i--) {
                removeParameter("Output " + ofToString(i + 1));
            }
            outputs.resize(newSize);
            lastOutputs.resize(newSize);
        } else if (newSize > oldSize) {
            outputs.resize(newSize);
            lastOutputs.resize(newSize);
            for (int i = oldSize; i < newSize; i++) {
                outputs[i].set("Output " + ofToString(i + 1), {0.0f}, {-FLT_MAX}, {FLT_MAX});
                addOutputParameter(outputs[i]);
            }
        }

        routeTo.setMax(vector<int>(1, newSize));
        // Don't automatically update outputs when changing number of outputs
        // Outputs will update on next input
    }

    void updateOutputs() {
        int n = (int)outputs.size();
        int inSize = (int)input.get().size();

        vector<vector<float>> newOutputs(n, vector<float>(inSize, 0.0f));

        bool isScalar = (routeTo.get().size() == 1);
        int defaultRoute = isScalar ? routeTo.get()[0] : -1;

        for (int i = 0; i < inSize; i++) {
            int route = isScalar ? defaultRoute
                                 : (i < (int)routeTo.get().size() ? routeTo.get()[i] : 1);
            route = ofClamp(route, 1, n);
            int routeIdx = route - 1;

            newOutputs[routeIdx][i] = input.get()[i];

            if (keep.get()) {
                for (int j = 0; j < n; j++) {
                    if (j != routeIdx) {
                        newOutputs[j][i] = ((int)lastOutputs[j].size() > i) ? lastOutputs[j][i] : 0.0f;
                    }
                }
            }
        }

        for (int j = 0; j < n; j++) {
            outputs[j].set(newOutputs[j]);
            lastOutputs[j] = newOutputs[j];
        }
    }

    ofParameter<int> numOutputs;
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> routeTo;
    ofParameter<bool> keep;
    ofParameter<bool> event;

    vector<ofParameter<vector<float>>> outputs;
    vector<vector<float>> lastOutputs;
    vector<float> lastInputValue;

    ofEventListeners listeners;
};

#endif /* distribute_h */
