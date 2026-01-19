#ifndef ignoreZeros_h
#define ignoreZeros_h

#include "ofxOceanodeNodeModel.h"

class ignoreZeros : public ofxOceanodeNodeModel {
public:
	ignoreZeros() : ofxOceanodeNodeModel("Ignore Zeros") {}

	void setup() override {
		description = "When an input value is zero, outputs the previous non-zero value instead. Each vector index maintains its own history.";
		
		addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

		listener = input.newListener([this](vector<float> &) { process(); });
	}

private:
	void process() {
		const vector<float>& in = input.get();
		
		// Resize history if input grew
		if (in.size() > lastNonZero.size()) {
			lastNonZero.resize(in.size(), 0.0f);
		}
		
		vector<float> out(in.size());
		
		for (size_t i = 0; i < in.size(); ++i) {
			if (in[i] != 0.0f) {
				lastNonZero[i] = in[i];
			}
			out[i] = lastNonZero[i];
		}
		
		output = out;
	}

	ofParameter<vector<float>> input;
	ofParameter<vector<float>> output;
	vector<float> lastNonZero;
	
	ofEventListener listener;
};

#endif /* ignoreZeros_h */
