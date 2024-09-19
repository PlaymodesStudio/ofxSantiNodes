#ifndef valuesChanged_h
#define valuesChanged_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <unordered_set>

class valuesChanged : public ofxOceanodeNodeModel {
public:
    valuesChanged() : ofxOceanodeNodeModel("Values Changed") {
        addParameter(input.set("Input",
                               {0.0f},
                               {-std::numeric_limits<float>::max()},
                               {std::numeric_limits<float>::max()}));
        addOutputParameter(changed.set("Changed",
                                       {0},
                                       {0},
                                       {1}));

       /* listeners.push(input.newListener([this](vector<float> &){
            detectChanges();
        }));
        */
    }

    void update(ofEventArgs &a)
    {
        detectChanges();
    }
    ~valuesChanged() {}

    
private:
    void detectChanges() {
        const auto& currentInput = input.get();
        vector<int> changedValues(currentInput.size(), 0);

        if (previousInput.size() != currentInput.size()) {
            // If sizes differ, mark all as changed
            std::fill(changedValues.begin(), changedValues.end(), 1);
        } else {
            std::unordered_set<float> prevSet(previousInput.begin(), previousInput.end());
            std::unordered_set<float> currSet(currentInput.begin(), currentInput.end());

            if (prevSet != currSet) {
                // Sets are different, so there's at least one change
                for (size_t i = 0; i < currentInput.size(); ++i) {
                    if (prevSet.find(currentInput[i]) == prevSet.end()) {
                        changedValues[i] = 1;
                    }
                }
            }
        }

        changed = changedValues;
        previousInput = currentInput;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<int>> changed;
    vector<float> previousInput;
    ofEventListeners listeners;
};

#endif /* valuesChanged_h */
