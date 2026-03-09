#ifndef sampleAndHold_h
#define sampleAndHold_h

#include "ofxOceanodeNodeModel.h"

class sampleAndHold : public ofxOceanodeNodeModel {
public:
	sampleAndHold() : ofxOceanodeNodeModel("Sample And Hold") {
		addInspectorParameter(numValues.set("Num Values", 1, 1, 8));
	}

	void setup() override {
		description = "Samples input values when corresponding gate is 1. In non-edge mode, continuously samples while gate is 1. In edge mode, only samples on rising edge (gate transition from 0 to 1).";

		addParameter(gatesInput.set("Gates", {0}, {0}, {1}));
		addParameter(strict.set("Edge", false));

		int n = numValues.get();
		valuesInputs.resize(n);
		outputs.resize(n);
		previousOutputs.resize(n);
		for(int i = 0; i < n; i++) {
			valuesInputs[i].set(valName(i), {0.0f}, {-FLT_MAX}, {FLT_MAX});
			addParameter(valuesInputs[i]);
			outputs[i].set(outName(i), {0.0f}, {-FLT_MAX}, {FLT_MAX});
			addOutputParameter(outputs[i]);
		}
		addOutputParameter(gateOutput.set("Gate Out", {0}, {0}, {1}));

		listeners.push(gatesInput.newListener([this](vector<int>&){ processInputs(); }));
		listeners.push(strict.newListener([this](bool&){ processInputs(); }));
		listeners.push(numValues.newListener([this](int&){ updateNumValues(); }));
		rebuildValueListeners();
	}

	void loadBeforeConnections(ofJson &json) override {
		deserializeParameter(json, numValues);
		updateNumValues();
	}

private:
	string valName(int i) { return i == 0 ? "Values" : "Values " + ofToString(i + 1); }
	string outName(int i) { return i == 0 ? "Output" : "Output " + ofToString(i + 1); }

	void rebuildValueListeners() {
		valueListeners.clear();
		for(int i = 0; i < (int)valuesInputs.size(); i++) {
			valueListeners.push_back(std::make_unique<ofEventListener>(
				valuesInputs[i].newListener([this](vector<float>&){ processInputs(); })
			));
		}
	}

	void updateNumValues() {
		int oldSize = (int)valuesInputs.size();
		int newSize = numValues.get();
		if(newSize == oldSize) return;

		valueListeners.clear();

		if(newSize < oldSize) {
			for(int i = oldSize - 1; i >= newSize; i--) {
				removeParameter(outName(i));
				removeParameter(valName(i));
			}
			valuesInputs.resize(newSize);
			outputs.resize(newSize);
			previousOutputs.resize(newSize);
		} else {
			// Remove Gate Out so new outputs are inserted before it
			removeParameter("Gate Out");
			valuesInputs.resize(newSize);
			outputs.resize(newSize);
			previousOutputs.resize(newSize);
			for(int i = oldSize; i < newSize; i++) {
				valuesInputs[i].set(valName(i), {0.0f}, {-FLT_MAX}, {FLT_MAX});
				addParameter(valuesInputs[i]);
				outputs[i].set(outName(i), {0.0f}, {-FLT_MAX}, {FLT_MAX});
				addOutputParameter(outputs[i]);
			}
			addOutputParameter(gateOutput);
		}

		rebuildValueListeners();
	}

	void processInputs() {
		const auto& gates = gatesInput.get();

		if(gates.empty()) {
			for(auto& out : outputs) out = vector<float>();
			return;
		}

		int n = (int)valuesInputs.size();
		if((int)previousOutputs.size() != n) previousOutputs.resize(n);
		if((int)previousGates.size() != (int)gates.size()) {
			previousGates.resize(gates.size(), 0);
		}

		for(int vi = 0; vi < n; vi++) {
			const auto& values = valuesInputs[vi].get();
			if(values.empty()) {
				outputs[vi] = vector<float>();
				continue;
			}

			size_t outputSize = values.size();
			auto& held = previousOutputs[vi];
			if(held.size() != outputSize) held.resize(outputSize, 0.0f);

			for(size_t i = 0; i < outputSize; i++) {
				size_t gateIndex = std::min(i, gates.size() - 1);
				int currentGate = gates[gateIndex];
				int prevGate = (gateIndex < previousGates.size()) ? previousGates[gateIndex] : 0;

				bool shouldSample = strict ? (currentGate == 1 && prevGate == 0)
				                           : (currentGate == 1);
				if(shouldSample) held[i] = values[i];
			}

			outputs[vi] = held;
		}

		// Output pitch first so downstream MIDI receives it before the gate fires
		previousGates = gates;

		// Mirror gate through after pitch — preserves full gate duration (HIGH and LOW)
		gateOutput = vector<int>(gates.begin(), gates.end());
	}

	ofParameter<int> numValues;
	ofParameter<vector<int>> gatesInput;
	ofParameter<bool> strict;
	ofParameter<vector<int>> gateOutput;

	vector<ofParameter<vector<float>>> valuesInputs;
	vector<ofParameter<vector<float>>> outputs;
	vector<vector<float>> previousOutputs;
	vector<int> previousGates;

	ofEventListeners listeners;
	vector<std::unique_ptr<ofEventListener>> valueListeners;
};

#endif /* sampleAndHold_h */
