//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023


#ifndef limitPolyphony_h
#define limitPolyphony_h

#include "ofxOceanodeNodeModel.h"
#include <deque>

class limitPolyphony : public ofxOceanodeNodeModel {
public:
    limitPolyphony() : ofxOceanodeNodeModel("Limit Polyphony") {}

    void setup() override {
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(maxPoly.set("Max Poly", 8, 1, 128));
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        listener = input.newListener([this](vector<float>& vf){
            // Check input size
            if (vf.size() != 128) {
                ofLogError() << "Input size must be 128";
                return;
            }

            // Clear output
            vector<float> tempOutput(128, 0.0f);

            // Keep track of notes that are currently on
            std::deque<int> noteOns;

            for (int i = 0; i < 128; ++i) {
                if (vf[i] != 0) {
                    // If note is on and wasn't on previously, add to deque
                    if (std::find(noteOns.begin(), noteOns.end(), i) == noteOns.end()) {
                        noteOns.push_back(i);
                    }
                } else {
                    // If note is off, remove from deque
                    noteOns.erase(std::remove(noteOns.begin(), noteOns.end(), i), noteOns.end());
                }
            }

            // If there are more notes on than allowed by maxPoly, turn oldest notes off until there are maxPoly notes on
            while (noteOns.size() > maxPoly) {
                int oldestNote = noteOns.front();
                vf[oldestNote] = 0;
                noteOns.pop_front();
            }

            // Pass updated notes to output
            output = vf;
        });
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<int> maxPoly;
    ofParameter<vector<float>> output;

    ofEventListener listener;
};

#endif /* limitPolyphony_h */
