#ifndef floatFilter_h
#define floatFilter_h

#include "ofxOceanodeNodeModel.h"

class vecFilter : public ofxOceanodeNodeModel {
public:
    vecFilter() : ofxOceanodeNodeModel("Float Filter") {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(indices.set("Indices", {0}, {0}, {1}));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listenerInput = input.newListener(this, &vecFilter::inputListener);
        listenerIndices = indices.newListener(this, &vecFilter::indicesListener);
    }

    ~vecFilter() {}

private:
    void inputListener(vector<float>& v) {
        updateOutput();
    }

    void indicesListener(vector<int>& v) {
        updateOutput();
    }

    void updateOutput() {
        auxOut.clear();
        for (size_t i = 0; i < input.get().size(); ++i) {
            if (i < indices.get().size() && indices.get()[i] == 1) {
                auxOut.push_back(input.get()[i]);
            }
        }
        output = auxOut;
    }

    ofEventListener listenerInput;
    ofEventListener listenerIndices;
    ofParameter<vector<float>> input;
    ofParameter<vector<int>> indices;
    ofParameter<vector<float>> output;
    vector<float> auxOut;
};

#endif /* floatFilter_h */

