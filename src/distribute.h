#ifndef distribute_h
#define distribute_h

#include "ofxOceanodeNodeModel.h"

class distribute : public ofxOceanodeNodeModel {
public:
    distribute() : ofxOceanodeNodeModel("Distribute"), routeUpdateNeeded(true) {
        description = "Routes each element of the incoming data to one of two outputs based on the 'Route To' setting per element. Optionally keeps the last value on the unoccupied output. The 'Event' toggle enables processing only on new unique inputs.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(routeTo.set("Route To", {1}, {1}, {2})); // Now a vector of integers
        addParameter(keep.set("Keep", false));
        addParameter(event.set("Event", false));

        addOutputParameter(output1.set("Output 1", {0}, {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(output2.set("Output 2", {0}, {-FLT_MAX}, {FLT_MAX}));

        prev_routeTo = routeTo.get();

        // Listener for input changes
        inputListener = input.newListener([this](vector<float> &in) {
            if (!event.get() || (event.get() && in != lastInputValue)) {
                updateOutputs();
                lastInputValue = in; // Store the last input value
            }
        });

        routeListener = routeTo.newListener([this](vector<int> &route) {
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
        vector<float> newOutput1(input.get().size(), 0);  // Initialize with zeros
        vector<float> newOutput2(input.get().size(), 0);  // Initialize with zeros

        int defaultRoute = routeTo.get().size() == 1 ? routeTo.get()[0] : -1;  // Check if routeTo is scalar

        for (size_t i = 0; i < input.get().size(); i++) {
            int route = defaultRoute != -1 ? defaultRoute : (i < routeTo.get().size() ? routeTo.get()[i] : 0); // Use defaultRoute if scalar, else use routeTo vector
            if (route == 1) {
                newOutput1[i] = input.get()[i];
                if (keep.get()) {
                    newOutput2[i] = lastOutput2.size() > i ? lastOutput2[i] : 0;  // Keep last value or use zero if not available
                }
            } else {
                newOutput2[i] = input.get()[i];
                if (keep.get()) {
                    newOutput1[i] = lastOutput1.size() > i ? lastOutput1[i] : 0;  // Keep last value or use zero if not available
                }
            }
        }

        output1.set(newOutput1);
        output2.set(newOutput2);
        lastOutput1 = newOutput1;  // Store the last outputs
        lastOutput2 = newOutput2;
    }

    // Member variables to store the last outputs
    vector<float> lastOutput1;
    vector<float> lastOutput2;
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> routeTo; // Changed to vector of ints
    ofParameter<bool> keep;
    ofParameter<bool> event; // The event toggle

    ofParameter<vector<float>> output1;
    ofParameter<vector<float>> output2;

    vector<int> prev_routeTo; // Updated to vector<int>
    vector<float> lastInputValue; // To keep track of the last received input

    ofEventListener inputListener;
    ofEventListener routeListener;
    ofEventListener keepListener;

    bool routeUpdateNeeded;
};

#endif /* distribute_h */
