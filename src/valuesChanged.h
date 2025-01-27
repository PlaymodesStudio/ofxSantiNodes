#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <limits>

class valuesChanged : public ofxOceanodeNodeModel {
public:
    valuesChanged() : ofxOceanodeNodeModel("Values Changed") {
        //--- Inputs
        addParameter(input.set("Input",
                               {0.0f},
                               {-std::numeric_limits<float>::max()},
                               {std::numeric_limits<float>::max()}));

        //--- Mode selection
        // 0 = Index-by-Index
        // 1 = Set-Based (original code)
        // 2 = Frequency-Based
        addParameter(mode.set("Mode", 0, 0, 2));

        //--- Outputs
        addOutputParameter(changed.set("Changed",
                                       {-1},
                                       {-1},
                                       {INT_MAX}));
        addOutputParameter(gates.set("Gates",
                                     {0},
                                     {0},
                                     {1}));

        //--- Listeners
        listeners.push(input.newListener([this](std::vector<float> &){
            detectChanges();
        }));
        listeners.push(mode.newListener([this](int &){
            detectChanges();
        }));

        description =
            "Detects changes in the input vector in three modes:\n\n"
            "0) Index-by-Index:\n"
            "   - Compares old vs new index-by-index.\n"
            "   - Reports changed or newly added/removed indices explicitly.\n"
            "   - Reordering => changes.\n\n"
            "1) Set-Based (Original):\n"
            "   - Uses a set of values to detect if a value completely\n"
            "     appears or disappears.\n"
            "   - Duplicates collapse; partial additions/removals aren't reported.\n"
            "   - Reordering is ignored.\n\n"
            "2) Frequency-Based:\n"
            "   - Compares the count of each value in old vs new.\n"
            "   - Reordering is ignored, but partial additions/removals are detected.\n"
            "   - Duplicates do matter (in quantity).";

    }

private:
    //------------------------------------------------------------------------
    void detectChanges() {
        const auto & current = input.get();
        const auto & prev    = previousInput;

        std::vector<int> changedIndices;
        std::vector<int> gatesOutput(current.size(), 0);

        switch(mode.get()){
            case 0:
                //========================================================
                // MODE 0: Index-by-Index
                //========================================================
            {
                size_t minSize = std::min(current.size(), prev.size());

                // Compare overlapping indices
                for(size_t i = 0; i < minSize; ++i){
                    if(current[i] != prev[i]) {
                        changedIndices.push_back(static_cast<int>(i));
                        gatesOutput[i] = 1;
                    }
                }

                // Anything new beyond old size
                for(size_t i = minSize; i < current.size(); ++i){
                    changedIndices.push_back(static_cast<int>(i));
                    gatesOutput[i] = 1;
                }

                // Anything removed beyond new size
                for(size_t i = minSize; i < prev.size(); ++i){
                    changedIndices.push_back(static_cast<int>(i));
                }
            }
            break;

            case 1:
                //========================================================
                // MODE 1: Set-Based (Original Logic)
                //========================================================
            {
                // Build sets of old/new
                std::unordered_set<float> currentValues(current.begin(), current.end());
                std::unordered_set<float> previousValues(prev.begin(), prev.end());

                // Map from oldValue => last index in 'prev'
                // (Original code overwrote duplicates with the last occurrence)
                std::unordered_map<float,int> prevValueToIndex;
                for(int i = 0; i < (int)prev.size(); ++i) {
                    prevValueToIndex[prev[i]] = i;
                }

                // Check for new/changed values (not in previous set)
                for(int i = 0; i < (int)current.size(); ++i){
                    if(previousValues.find(current[i]) == previousValues.end()){
                        changedIndices.push_back(i);
                        gatesOutput[i] = 1;
                    }
                }

                // Check for removed values (in previous set but not in current set)
                for(const auto & val : previousValues){
                    if(currentValues.find(val) == currentValues.end()){
                        // The original code used the mapped index
                        int removedIndex = prevValueToIndex[val];
                        // If that index is within the new vector, mark it
                        if(removedIndex < (int)current.size()){
                            changedIndices.push_back(removedIndex);
                            gatesOutput[removedIndex] = 1;
                        }
                    }
                }
            }
            break;

            case 2:
                //========================================================
                // MODE 2: Frequency-Based
                //========================================================
            {
                // OldIndices: value -> vector of old indices
                // NewIndices: value -> vector of new indices
                std::unordered_map<float, std::vector<int>> oldIndices, newIndices;

                for(int i = 0; i < (int)prev.size(); ++i){
                    oldIndices[prev[i]].push_back(i);
                }
                for(int i = 0; i < (int)current.size(); ++i){
                    newIndices[current[i]].push_back(i);
                }

                // Union of values in oldIndices + newIndices
                std::unordered_map<float,int> unionHelper;
                for(const auto & kv : oldIndices) unionHelper[kv.first] = 1;
                for(const auto & kv : newIndices) unionHelper[kv.first] = 1;

                // For each value in the union, compare oldCount vs newCount
                for(const auto & kv : unionHelper){
                    float val = kv.first;
                    auto & oldVec = oldIndices[val]; // empty if not found
                    auto & newVec = newIndices[val]; // empty if not found

                    int oldCount = (int)oldVec.size();
                    int newCount = (int)newVec.size();

                    int diff = newCount - oldCount;

                    // If diff>0 => that many new occurrences
                    if(diff > 0) {
                        // The first 'oldCount' are matched, the rest are new
                        for(int i = oldCount; i < newCount; ++i){
                            int idx = newVec[i];
                            changedIndices.push_back(idx);
                            gatesOutput[idx] = 1;
                        }
                    }
                    // If diff<0 => that many removed occurrences
                    else if(diff < 0) {
                        // The first 'newCount' are matched, leftover old are removed
                        for(int i = newCount; i < oldCount; ++i){
                            int idx = oldVec[i];
                            changedIndices.push_back(idx);
                        }
                    }
                }
            }
            break;
        }

        // Finalize changes
        if(changedIndices.empty()){
            changed = {-1};
        } else {
            std::sort(changedIndices.begin(), changedIndices.end());
            changed = changedIndices;
        }

        gates = gatesOutput;
        previousInput = current;
    }

    //--- Parameters
    ofParameter<std::vector<float>> input;
    ofParameter<int>                mode;    // 0=Index-by-Index, 1=Set, 2=Freq
    ofParameter<std::vector<int>>   changed;
    ofParameter<std::vector<int>>   gates;

    //--- State
    std::vector<float> previousInput;
    ofEventListeners listeners;
};
