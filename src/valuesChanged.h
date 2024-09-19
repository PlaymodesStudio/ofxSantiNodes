#ifndef valuesChanged_h
#define valuesChanged_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

class valuesChanged : public ofxOceanodeNodeModel {
public:
    valuesChanged() : ofxOceanodeNodeModel("Values Changed") {
        addParameter(input.set("Input",
                               {0.0f},
                               {-std::numeric_limits<float>::max()},
                               {std::numeric_limits<float>::max()}));
        addOutputParameter(changed.set("Changed",
                                       {-1},
                                       {-1},
                                       {INT_MAX}));
        addOutputParameter(gates.set("Gates",
                                     {0},
                                     {0},
                                     {1}));

        listeners.push(input.newListener([this](vector<float> &){
            detectChanges();
        }));
        
        description = "Detects changes in the input vector, including additions, removals, and value changes. Outputs indices of changed values or -1 if no changes occurred. Also outputs a 'gates' vector with 1s for changed values and 0s for unchanged values.";
    }

    ~valuesChanged() {}

private:
    void detectChanges() {
        const auto& currentInput = input.get();
        std::unordered_set<int> changedIndices;
        vector<int> gatesOutput(currentInput.size(), 0);

        std::unordered_map<float, int> prevValueToIndex;
        for (int i = 0; i < previousInput.size(); ++i) {
            prevValueToIndex[previousInput[i]] = i;
        }

        std::unordered_set<float> currentValues(currentInput.begin(), currentInput.end());
        std::unordered_set<float> previousValues(previousInput.begin(), previousInput.end());

        // Check for new or changed values
        for (int i = 0; i < currentInput.size(); ++i) {
            if (previousValues.find(currentInput[i]) == previousValues.end()) {
                changedIndices.insert(i);
                gatesOutput[i] = 1;
            }
        }

        // Check for removed values
        for (const auto& prevValue : previousValues) {
            if (currentValues.find(prevValue) == currentValues.end()) {
                int removedIndex = prevValueToIndex[prevValue];
                if (removedIndex < currentInput.size()) {
                    changedIndices.insert(removedIndex);
                    gatesOutput[removedIndex] = 1;
                }
            }
        }

        if (changedIndices.empty()) {
            changed = {-1};  // No changes detected
        } else {
            vector<int> changedIndicesVector(changedIndices.begin(), changedIndices.end());
            std::sort(changedIndicesVector.begin(), changedIndicesVector.end());
            changed = changedIndicesVector;
        }

        gates = gatesOutput;
        previousInput = currentInput;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<int>> changed;
    ofParameter<vector<int>> gates;
    vector<float> previousInput;
    ofEventListeners listeners;
};

#endif /* valuesChanged_h */
