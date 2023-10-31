#ifndef EVENT_COUNTER_H
#define EVENT_COUNTER_H

#include "ofxOceanodeNodeModel.h"

class eventCounter : public ofxOceanodeNodeModel {
public:
    eventCounter() : ofxOceanodeNodeModel("Event Counter"), resetOnNextEvent(false) {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(vec.set("Vec", false));
        addParameter(resetButton.set("Reset"));
        addParameter(resetNext.set("Reset Next")); // Adding the 'resetNext' void parameter
        addOutputParameter(count.set("Count", {0}, {0}, {INT_MAX}));
        addOutputParameter(tick.set("Tick")); // Adding the 'tick' void output
        
        description = "Counts the number of events that have reached the input. Outputs as a vector of integers, and a void tick for each new event.";
        
        listener = input.newListener([this](vector<float> &vf) {
            if(resetOnNextEvent) {
                eventCount = 0;
                resetOnNextEvent = false;
            } else {
                eventCount++;
            }
            tick.trigger(); // Trigger the tick when a new event is counted
            if (vec || vf.size() == 1) {
                vector<int> vecOut(vf.size(), eventCount);
                count = vecOut;
            } else {
                vector<int> singleValue(1, eventCount);
                count = singleValue;
            }
        });
        
        resetListener = resetButton.newListener([this]() {
            eventCount = 0;
            vector<int> resetValue(1, eventCount);
            count = resetValue;
        });
        
        resetNextListener = resetNext.newListener([this]() {
            resetOnNextEvent = true;
        });
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<bool> vec;
    ofParameter<void> resetButton;
    ofParameter<void> resetNext; // Declaration for 'resetNext' parameter
    ofParameter<vector<int>> count;
    ofParameter<void> tick; // Declaration for 'tick' output

    int eventCount = 0; // To keep track of the number of events
    bool resetOnNextEvent; // Flag to check if reset is needed on next event
    ofEventListener listener;
    ofEventListener resetListener;
    ofEventListener resetNextListener; // Listener for 'resetNext'
};

#endif /* EVENT_COUNTER_H */
