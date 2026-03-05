// voidSwitch.h
#pragma once

#include "ofxOceanodeNodeModel.h"

class voidSwitch : public ofxOceanodeNodeModel {
public:
	voidSwitch() : ofxOceanodeNodeModel("Void Switch") {
		description = "Routes one of N void trigger inputs to a single void trigger output. The 'Active' parameter selects which input is live.";

		addInspectorParameter(numInputs.set("Num Inputs", 2, 1, 16));
	}

	void setup() override {
		addParameter(active.set("Active", 0, 0, numInputs.get() - 1));
		addOutputParameter(output.set("Output"));

		buildInputs(numInputs.get());

		listeners.push(numInputs.newListener([this](int &) {
			updateNumInputs();
		}));
	}

private:
	// ---------- Inspector ----------
	ofParameter<int> numInputs;

	// ---------- Parameters ----------
	ofParameter<int>  active;
	ofParameter<void> output;

	// ---------- Dynamic void inputs ----------
	// Both ofParameter<void> and ofEventListener are non-copyable/non-movable,
	// so we heap-allocate both via unique_ptr to keep them in vectors safely.
	std::vector<std::unique_ptr<ofParameter<void>>>  inputs;
	std::vector<std::unique_ptr<ofEventListener>>    inputListeners;

	ofEventListeners listeners;

	// ---------- Helpers ----------

	void addInput(int i) {
		auto param = std::make_unique<ofParameter<void>>();
		param->set("Input " + ofToString(i + 1));
		addParameter(*param);

		auto lst = std::make_unique<ofEventListener>(
			param->newListener([this, i]() {
				if (i == active.get()) {
					output.trigger();
				}
			})
		);

		inputs.push_back(std::move(param));
		inputListeners.push_back(std::move(lst));
	}

	void buildInputs(int n) {
		inputs.clear();
		inputListeners.clear();
		for (int i = 0; i < n; i++) {
			addInput(i);
		}
	}

	void updateNumInputs() {
		int oldSize = (int)inputs.size();
		int newSize = numInputs.get();

		if (newSize == oldSize) return;

		if (newSize < oldSize) {
			for (int i = oldSize - 1; i >= newSize; i--) {
				inputListeners[i].reset(); // disconnect listener first
				removeParameter("Input " + ofToString(i + 1));
			}
			inputs.resize(newSize);
			inputListeners.resize(newSize);
		} else {
			for (int i = oldSize; i < newSize; i++) {
				addInput(i);
			}
		}

		active.setMax(newSize - 1);
		if (active.get() > newSize - 1) active.set(newSize - 1);
	}
};
