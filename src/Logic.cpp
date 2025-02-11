//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#include "Logic.h"

Logic::Logic() : ofxOceanodeNodeModel("Logic") {}

void Logic::setup() {
    addParameter(input1.set("Input 1", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(input2.set("Input 2", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameterDropdown(operation, "Op", 0,
      {">", ">=", "<", "<=", "==", "!=", "&&", "||", "!>", "!<", "!>=", "!<="});
    addOutputParameter(output.set("Output", {0}, {0}, {1}));

    listeners.push(input1.newListener([this](vector<float>& vf) {
        computeLogic();
    }));
    listeners.push(input2.newListener([this](vector<float>& vf) {
        computeLogic();
    }));
}

void Logic::computeLogic() {
	if(input1.get().size()!=0 && input2.get().size()!=0)
	{
		auto getValueForIndex = [](const vector<float> &vf, int i) -> float {
			if(i < vf.size()){
				return vf[i];
			}else{
				return vf[0];
			}
		};

		vector<float> result;
		vector<string> operations = {">", ">=", "<", "<=", "==", "!=", "&&", "||", "!>", "!<", "!>=", "!<="};

		for (size_t i = 0; i < std::max(input1->size(), input2->size()); ++i) {
			bool logicResult = false;
			string op = operations[operation.get()];  // Get operation string from operations vector
			float val1 = getValueForIndex(input1, i);
			float val2 = getValueForIndex(input2, i);
			if (op == ">") logicResult = val1 > val2;
			else if (op == ">=") logicResult = val1 >= val2;
			else if (op == "<") logicResult = val1 < val2;
			else if (op == "<=") logicResult = val1 <= val2;
			else if (op == "==") logicResult = val1 == val2;
			else if (op == "!=") logicResult = val1 != val2;
			else if (op == "&&") logicResult = static_cast<bool>(val1) && static_cast<bool>(val2);
			else if (op == "||") logicResult = static_cast<bool>(val1) || static_cast<bool>(val2);
			else if (op == "!>") logicResult = !(val1 > val2);
			else if (op == "!<") logicResult = !(val1 < val2);
			else if (op == "!>=") logicResult = !(val1 >= val2);
			else if (op == "!<=") logicResult = !(val1 <= val2);
			result.push_back(logicResult ? 1.0f : 0.0f);
		}
		output = result;
	}
}
