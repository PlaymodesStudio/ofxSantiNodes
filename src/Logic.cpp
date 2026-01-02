//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#include "Logic.h"

Logic::Logic() : ofxOceanodeNodeModel("Logic") {}

void Logic::setup() {
	addParameter(input1.set("Input 1", {0}, {-FLT_MAX}, {FLT_MAX}));
	addParameter(input2.set("Input 2", {0}, {-FLT_MAX}, {FLT_MAX}));
	addParameterDropdown(operation, "Op", 0,
	  {">", ">=", "<", "<=", "==", "!=", "&&", "||", "!>", "!<", "!>=", "!<=",
	   "XOR", "NAND", "NOR", "XNOR", "NOT1", "NOT2"});
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
		vector<string> operations = {">", ">=", "<", "<=", "==", "!=", "&&", "||",
									 "!>", "!<", "!>=", "!<=",
									 "XOR", "NAND", "NOR", "XNOR", "NOT1", "NOT2"};

		for (size_t i = 0; i < std::max(input1->size(), input2->size()); ++i) {
			bool logicResult = false;
			string op = operations[operation.get()];
			float val1 = getValueForIndex(input1, i);
			float val2 = getValueForIndex(input2, i);
			bool bool1 = static_cast<bool>(val1);
			bool bool2 = static_cast<bool>(val2);
			
			if (op == ">") logicResult = val1 > val2;
			else if (op == ">=") logicResult = val1 >= val2;
			else if (op == "<") logicResult = val1 < val2;
			else if (op == "<=") logicResult = val1 <= val2;
			else if (op == "==") logicResult = val1 == val2;
			else if (op == "!=") logicResult = val1 != val2;
			else if (op == "&&") logicResult = bool1 && bool2;
			else if (op == "||") logicResult = bool1 || bool2;
			else if (op == "!>") logicResult = !(val1 > val2);
			else if (op == "!<") logicResult = !(val1 < val2);
			else if (op == "!>=") logicResult = !(val1 >= val2);
			else if (op == "!<=") logicResult = !(val1 <= val2);
			else if (op == "XOR") logicResult = bool1 != bool2;  // XOR: true if different
			else if (op == "NAND") logicResult = !(bool1 && bool2);  // NAND: not AND
			else if (op == "NOR") logicResult = !(bool1 || bool2);  // NOR: not OR
			else if (op == "XNOR") logicResult = bool1 == bool2;  // XNOR: true if same
			else if (op == "NOT1") logicResult = !bool1;  // NOT of input 1
			else if (op == "NOT2") logicResult = !bool2;  // NOT of input 2
			
			result.push_back(logicResult ? 1.0f : 0.0f);
		}
		output = result;
	}
}
