#ifndef distribute_h
#define distribute_h

#include "ofxOceanodeNodeModel.h"

class distribute : public ofxOceanodeNodeModel {
public:
    distribute() : ofxOceanodeNodeModel("Distribute"), routeUpdateNeeded(true) {
        description = "Routes incoming data to one of two outputs based on the 'Route To' setting. Optionally keeps the last value on the unoccupied output. The 'Event' toggle enables processing only on new unique inputs.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(routeTo.set("Route To", 1, 1, 2));
        addParameter(keep.set("Keep", false));
        addParameter(event.set("Event", false));  // The event toggle
        
        addOutputParameter(output1.set("Output 1", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output2.set("Output 2", {0}, {-FLT_MAX}, {FLT_MAX}));

        prev_routeTo = routeTo.get();

        // Listener for input changes
        inputListener = input.newListener([this](vector<float> &in) {
            if (!event.get() || (event.get() && in != lastInputValue)) {  // Process always when event is off, or on a new event when event is on
                updateOutputs();
                lastInputValue = in; // Store the last input value
            }
        });

        routeListener = routeTo.newListener([this](int &route) {
            if(routeUpdateNeeded || !event.get()) {
                updateOutputs();
            }
            routeUpdateNeeded = false; // Reset after each update
        });

        keepListener = keep.newListener([this](bool &k) {
            updateOutputs();
        });
    }

private:
    void updateOutputs() {
        if(routeTo == 1) {
            output1 = input.get();
            if(!keep.get()) {
                output2 = {0};
            }
        } else if(routeTo == 2) {
            output2 = input.get();
            if(!keep.get()) {
                output1 = {0};
            }
        }
    }

    ofParameter<vector<float>> input;
    ofParameter<int> routeTo;
    ofParameter<bool> keep;
    ofParameter<bool> event;  // The event toggle

    ofParameter<vector<float>> output1;
    ofParameter<vector<float>> output2;

    int prev_routeTo;
    vector<float> lastInputValue;  // To keep track of the last received input

    ofEventListener inputListener;
    ofEventListener routeListener;
    ofEventListener keepListener;

    bool routeUpdateNeeded;
};

#endif /* distribute_h */
