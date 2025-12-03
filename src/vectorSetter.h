#ifndef vectorSetter_h
#define vectorSetter_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>

class vectorSetter : public ofxOceanodeNodeModel {
public:
    vectorSetter() : ofxOceanodeNodeModel("Vector Setter") {
        description = "This node takes an input vector of floats and lets you replace specific elements at given indices, then outputs the modified vector. You choose which positions to change with the “Index” vector and what values to write with the “Set To” vector. If “Set To” has only one value, that value is used for all indices. If it has fewer values than indices, the last value is reused to fill the rest. If it has more values, the extra ones are ignored.\n\nWhen “Accum” is off, each calculation starts from the current input vector. When “Accum” is on, the node starts from the previous output (as long as it’s the same size as the input), so earlier edits stay in place until you overwrite them. The “Update Mode” menu controls what triggers a recalculation: “OnIndex” reacts to index changes, “OnValue” reacts to value changes, and “Always” reacts to both, while input changes always update the result.";

        addParameter(input.set("Input",
                             vector<float>(1, 0.0f),
                             vector<float>(1, -std::numeric_limits<float>::max()),
                             vector<float>(1, std::numeric_limits<float>::max())));
        addParameter(index.set("Index",
                             vector<int>(1, -1),
                             vector<int>(1, -1),
                             vector<int>(1, INT_MAX)));
        addParameter(setTo.set("Set To",
                             vector<float>(1, 0.0f),
                             vector<float>(1, -std::numeric_limits<float>::max()),
                             vector<float>(1, std::numeric_limits<float>::max())));
		addParameter(accum.set("Accum", false));
		//addParameter(accOnValue.set("Acc.OnValue", false));
		addParameterDropdown(accumType,"Update Mode",2,{"OnIndex", "OnValue", "Always"});
        addOutputParameter(output.set("Output",
                                    vector<float>(1, 0.0f),
                                    vector<float>(1, -std::numeric_limits<float>::max()),
                                    vector<float>(1, std::numeric_limits<float>::max())));

        // Store previous output for accumulation
        previousOutput = vector<float>(1, 0.0f);

        listeners.push(input.newListener([this](vector<float> &v){
			updateOutput();
        }));
        listeners.push(index.newListener([this](vector<int> &v){
            if((accumType==0)||(accumType==2)) updateOutput();
        }));
        listeners.push(setTo.newListener([this](vector<float> &v){
			if((accumType==1)||(accumType==2)) updateOutput();
        }));
//        listeners.push(accumType.newListener([this](int &i){
//			if((accumType!=0)) updateOutput(true);
//			else updateOutput(false);
//        }));
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> index;
    ofParameter<vector<float>> setTo;
    ofParameter<bool> accum;
    //ofParameter<bool> accOnValue;
    ofParameter<vector<float>> output;
	ofParameter<int> accumType;
	vector<float> previousOutput;
	

    ofEventListeners listeners;

    void updateOutput() {
        vector<float> result;
        
        if (accum) {
            // If accumulating, start with previous output if it matches input size
            if (previousOutput.size() == input.get().size()) {
                result = previousOutput;
            } else {
                result = input.get();
            }
        } else {
            // If not accumulating, start fresh with input
            result = input.get();
        }

        const vector<int>& indices = index.get();
        vector<float> values = setTo.get();

        if(values.size() != 0) {
            // Resize and replicate values if necessary
            if (values.size() == 1) {
                // If setTo is a scalar, replicate it to match the size of indices
                values.resize(indices.size(), values[0]);
            } else if (values.size() < indices.size()) {
                // If setTo is smaller than indices, replicate the last value
                float lastValue = values.back();
                values.resize(indices.size(), lastValue);
            } else if (values.size() > indices.size()) {
                // If setTo is larger than indices, truncate it
                values.resize(indices.size());
            }

            // Apply the changes
            for (size_t i = 0; i < indices.size(); ++i) {
                if (indices[i] >= 0 && indices[i] < result.size()) {
                    result[indices[i]] = values[i];
                }
            }

            output.set(result);
            previousOutput = result;  // Store for next accumulation
        }
    }
};

#endif /* vectorSetter_h */
