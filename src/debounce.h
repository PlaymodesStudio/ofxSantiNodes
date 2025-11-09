#pragma once
#include "ofxOceanodeNodeModel.h"
#include <cmath>

class debounce : public ofxOceanodeNodeModel {
public:
	debounce() : ofxOceanodeNodeModel("debounce") {}

	void setup() override {
		// entrada (vector, però pot ser 1 escalar)
		addParameter(input.set("Input", {0.0f}, {FLT_MIN}, {FLT_MAX}));

		// quants frames seguits ha d’estar el mateix valor
		addParameter(requiredFrames.set("Frames", 3, 1, 200));

		// tolerància per dir "és el mateix valor"
		addParameter(tolerance.set("Tolerance", 0.0001f, 0.0f, 1.0f));

		// sortida
		addOutputParameter(output.set("Output", {0.0f}, {FLT_MIN}, {FLT_MAX}));

		// estat inicial
		currentStable = input.get();   // agafem el que hi hagi
		candidate     = currentStable;
		stableCounter = 0;

		// listener per si canvia la mida de l'input
		inputListener = input.newListener([this](vector<float> &v){
			resizeInternal(v.size());
		});
	}

	void update(ofEventArgs &e) override {
		auto &in = input.get();

		// assegurem mides
		if(in.size() != currentStable.size()){
			resizeInternal(in.size());
		}

		// 1) mirem si l'input és el mateix que el candidat dins la tolerància
		if(vectorsClose(in, candidate, tolerance)){
			stableCounter++;
		} else {
			// nou valor → esdevé candidat
			candidate = in;
			stableCounter = 1;
		}

		// 2) només quan porta prou frames igual → el deixem passar
		if(stableCounter >= requiredFrames){
			currentStable = candidate;
		}

		// 3) publiquem sempre l’últim valor estable
		output = currentStable;
	}

private:
	ofParameter<vector<float>> input;
	ofParameter<int>           requiredFrames;
	ofParameter<float>         tolerance;
	ofParameter<vector<float>> output;

	vector<float> currentStable;   // el que efectivament veu el món
	vector<float> candidate;       // el que està intentant entrar
	int           stableCounter = 0;

	ofEventListener inputListener;

	void resizeInternal(size_t sz){
		currentStable.resize(sz, 0.0f);
		candidate.resize(sz, 0.0f);

		auto out = output.get();
		out.resize(sz, 0.0f);
		output.set(out);
	}

	bool vectorsClose(const vector<float> &a, const vector<float> &b, float tol){
		if(a.size() != b.size()) return false;
		for(size_t i = 0; i < a.size(); ++i){
			if(std::fabs(a[i] - b[i]) > tol){
				return false;
			}
		}
		return true;
	}
};
