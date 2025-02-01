#ifndef risingEdgeReindexer_h
#define risingEdgeReindexer_h

#include "ofxOceanodeNodeModel.h"

class risingEdgeReindexer : public ofxOceanodeNodeModel {
public:
    risingEdgeReindexer() : ofxOceanodeNodeModel("Rising Edge Reindexer") {}

    void setup() override {
        description = "Tracks rising edges (0 to non-zero transitions) in the input vector and outputs indices in order of activation. When a value returns to 0, its position becomes available for new rising edges while maintaining positions of other active values.";
        
        addParameter(input.set("Input", {0}, {0}, {1}));
        addParameter(fixedSize.set("Fixed Size", true));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));
        addOutputParameter(idx.set("Idx", {-1}, {-1}, {INT_MAX}));
        
        // Store previous input for edge detection
        prevInput = {0};
        
        // Listen for input changes
        listeners.push(input.newListener([this](vector<float> &vals){
            processInput();
        }));

        listeners.push(fixedSize.newListener([this](bool &val){
            processInput(); // Reprocess to update output format
        }));
    }

private:
    void processInput() {
        const auto& currentInput = input.get();
        
        // Ensure prevInput matches current size
        if(prevInput.size() != currentInput.size()) {
            prevInput.resize(currentInput.size(), 0.0f);
            if(fixedSize) {
                positionMap.resize(currentInput.size(), -1); // Reset position map
            }
        }
        
        if(fixedSize) {
            // Fixed-size mode with position tracking
            
            // First, handle zeros (deactivations)
            for(size_t i = 0; i < currentInput.size(); i++) {
                if(prevInput[i] != 0 && currentInput[i] == 0) {
                    // Find and clear this index's position
                    for(size_t pos = 0; pos < positionMap.size(); pos++) {
                        if(positionMap[pos] == static_cast<int>(i)) {
                            positionMap[pos] = -1;
                            break;
                        }
                    }
                }
            }
            
            // Then handle rising edges
            for(size_t i = 0; i < currentInput.size(); i++) {
                if(prevInput[i] == 0 && currentInput[i] != 0) {
                    // Find first available position
                    for(size_t pos = 0; pos < positionMap.size(); pos++) {
                        if(positionMap[pos] == -1) {
                            positionMap[pos] = i;
                            break;
                        }
                    }
                }
            }
            
            // Create output based on position map
            vector<float> outputVals(currentInput.size(), 0.0f);
            vector<int> idxVals(currentInput.size(), -1);
            
            for(size_t pos = 0; pos < positionMap.size(); pos++) {
                if(positionMap[pos] != -1) {
                    outputVals[pos] = currentInput[positionMap[pos]];
                    idxVals[pos] = positionMap[pos];
                }
            }
            
            output = outputVals;
            idx = idxVals;
            
        } else {
            // Dynamic-size mode
            // Check for new rising edges and process zeros
            for(size_t i = 0; i < currentInput.size(); i++) {
                // Rising edge detection
                if(prevInput[i] == 0 && currentInput[i] != 0) {
                    // Remove this index if it exists already
                    activeIndices.erase(
                        std::remove(activeIndices.begin(), activeIndices.end(), i),
                        activeIndices.end()
                    );
                    // Add to end of sequence
                    activeIndices.push_back(i);
                }
                // Zero detection
                else if(prevInput[i] != 0 && currentInput[i] == 0) {
                    // Remove from sequence
                    activeIndices.erase(
                        std::remove(activeIndices.begin(), activeIndices.end(), i),
                        activeIndices.end()
                    );
                }
            }
            
            // Create dynamic-size output
            vector<float> outputVals;
            vector<int> idxVals;
            
            for(size_t originalIdx : activeIndices) {
                if(originalIdx < currentInput.size()) {
                    outputVals.push_back(currentInput[originalIdx]);
                    idxVals.push_back(originalIdx);
                }
            }
            
            output = outputVals;
            idx = idxVals;
        }
        
        // Update previous input
        prevInput = currentInput;
    }

    ofParameter<vector<float>> input;
    ofParameter<vector<float>> output;
    ofParameter<vector<int>> idx;
    ofParameter<bool> fixedSize;
    ofEventListeners listeners;
    
    vector<float> prevInput;
    vector<size_t> activeIndices;  // Used for dynamic-size mode
    vector<int> positionMap;       // Used for fixed-size mode
};

#endif /* risingEdgeReindexer_h */
