#ifndef formula_h
#define formula_h

#include "ofxOceanodeNodeModel.h"
#include <string>
#include <regex>
#include <cmath>
#include <stack>
#include <sstream>
#include <map>
#include <cctype>
#include <limits>
#include <functional>
#include <optional>
#include <set>
#include <algorithm>
#include <utility>


class formula : public ofxOceanodeNodeModel {
public:
	formula() : ofxOceanodeNodeModel("Formula") {}

	void setup() override {
		description = "Math formula evaluator. Edit the formula on the node. Use $1, $2, $3... for inputs. Supports +,-,*,/,%,^,( ), sin/cos/tan, atan2, sqrt, abs, pow, exp, log, min/max, clamp, step, smoothstep, floor/ceil/round. Scalars broadcast over vectors. Conditional evaluation using if(cond,true,false)";


		// Inspector-only params
		addInspectorParameter(numInputs.set("Num Inputs", 2, 1, 16));
		numInputsListener = numInputs.newListener([this](int &i){
			int current = (int)inputParameters.size();
			if(i > current){
				for(int k = current; k < i; ++k) addInputParameter(k);
			}else if(i < current){
				for(int k = current - 1; k >= i; --k) removeInputParameter(k);
			}
			previousNumInputs = i; // keep your tracker in sync
		});

		// Show the formula text field on the NODE GUI
		addInspectorParameter(formulaString.set("Formula", "($1 + $2) / 2"));

		// Output
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

		// Inspector controls for the multiline editor
		addInspectorParameter(editorLines.set("Editor Lines", 3, 1, 40));
		addInspectorParameter(editorFontSize.set("Editor Font Size", 28.0f, 14.0f, 28.0f));

		// Preload editable buffer
		formulaBuf = formulaString.get();

		// Keep buffer in sync when param changes elsewhere
		formulaStrListener = formulaString.newListener([this](std::string &s){
			if(s != formulaBuf) formulaBuf = s;
		});

	
		addCustomRegion(formulaEditorRegion, [this](){

			const float PADDING = 6.0f;

			// Compute height in pixels based on text lines
			const float baseLineHeight = ImGui::GetTextLineHeightWithSpacing();
			const int   lines          = editorLines.get();
			const float boxH           = lines * baseLineHeight;

			// Width from parameter (or node size)
			const float boxW = 240;

			// Begin fixed-size child (this prevents spilling)
			ImGui::BeginChild("FormulaEditor",
							  ImVec2(boxW, boxH + 2*PADDING),
							  true,
							  ImGuiWindowFlags_None);

			// Temporarily scale font for this child only
			const float basePx = 14.0f;
			const float scale  = editorFontSize.get() / basePx;
			ImGui::SetWindowFontScale(scale);

			// Cursor setup
			ImGui::SetCursorPos(ImVec2(PADDING, PADDING));

			// Build buffer
			static std::vector<char> buf;
			buf.assign(formulaBuf.begin(), formulaBuf.end());
			buf.push_back('\0');

			// Actual multiline field size (subtract padding)
			ImVec2 inputSize(boxW - 2*PADDING, boxH);

			bool changed = ImGui::InputTextMultiline(
				"##formulaML",
				buf.data(), buf.size(),
				inputSize,
				ImGuiInputTextFlags_AllowTabInput |
				ImGuiInputTextFlags_CallbackResize,
				[](ImGuiInputTextCallbackData* data)->int {
					if(data->EventFlag == ImGuiInputTextFlags_CallbackResize){
						auto* v = reinterpret_cast<std::vector<char>*>(data->UserData);
						v->resize(data->BufTextLen + 1);
						data->Buf = v->data();
					}
					return 0;
				},
				(void*)&buf
			);

			if(changed){
				formulaBuf.assign(buf.data(), std::strlen(buf.data()));
				if(formulaBuf != formulaString.get()){
					formulaString.set(formulaBuf);
					rebuildEvaluator();
					calculate();
				}
			}

			// Reset font scale
			ImGui::SetWindowFontScale(1.0f);
			ImGui::EndChild();
		});

		
		formulaBuf = formulaString.get();
		formulaString.addListener(this, &formula::onFormulaParamChanged);



		// Initialize
		updateInputs();
		rebuildEvaluator();
	}

	// Override loadBeforeConnections to ensure inputs are created before connections are loaded
	void loadBeforeConnections(ofJson &json) override {
		// Load the numInputs parameter from the preset
		deserializeParameter(json, numInputs);
		// Force update inputs immediately to ensure they exist before connections are loaded
		updateInputs();
	}

	void update(ofEventArgs &args) override {
		if(formulaString.get() != previousFormula) {
			previousFormula = formulaString.get();
			rebuildEvaluator();
			calculate();
		}
	}

private:
	// Parameters
	ofParameter<int> numInputs;
	ofParameter<string> formulaString;
	ofParameter<vector<float>> output;

	customGuiRegion formulaEditorRegion;
	mutable std::string formulaBuf;
	ofEventListener formulaStrListener;
	ofParameter<int>   editorLines;
	ofParameter<float> editorFontSize;

	ofEventListener numInputsListener;

	// Change tracking
	int    previousNumInputs = -1;
	string previousFormula   = "";

	// Dynamic input storage
	std::map<int, std::shared_ptr<ofxOceanodeParameter<vector<float>>>> inputParameters;
	std::map<int, std::shared_ptr<ofParameter<vector<float>>>>          inputParamRefs;
	std::map<int, ofEventListener>                                      inputListeners;

	// Evaluator state
	struct Token {
		enum Type { Number, Identifier, Operator, LParen, RParen, Comma } type;
		std::string text;
		float value = 0.0f;
		int precedence = 0;
		bool rightAssoc = false;
		bool unary = false;
	};

	using RPN = std::vector<Token>;

	RPN rpn;                    // compiled expression
	bool formulaValid = false;  // compilation status
	std::string lastError;

	//
	void onFormulaParamChanged(std::string &s){
		if(s != formulaBuf) formulaBuf = s;
	}

	// ---------- Inputs ----------
	void updateInputs() {
		int newNumInputs = numInputs.get();
		int currentNumInputs = (int)inputParameters.size();

		if(newNumInputs > currentNumInputs) {
			for(int i = currentNumInputs; i < newNumInputs; i++) addInputParameter(i);
		} else if(newNumInputs < currentNumInputs) {
			for(int i = currentNumInputs - 1; i >= newNumInputs; i--) removeInputParameter(i);
		}
		calculate();
	}

	void addInputParameter(int index) {
		std::string name = "$" + ofToString(index + 1);

		auto paramRef = std::make_shared<ofParameter<vector<float>>>();
		paramRef->set(name, {0}, {-FLT_MAX}, {FLT_MAX});
		inputParamRefs[index] = paramRef;

		auto oceaParam = addParameter(*paramRef);
		inputParameters[index] = oceaParam;

		inputListeners[index] = paramRef->newListener([this](vector<float>&){ calculate(); });
	}

	void removeInputParameter(int index) {
		std::string name = "$" + ofToString(index + 1);
		inputListeners.erase(index);
		removeParameter(name);
		inputParameters.erase(index);
		inputParamRefs.erase(index);
	}

	// ---------- Public evaluation ----------
	void calculate() {
		if(!formulaValid || inputParameters.empty()) {
			output = {0};
			return;
		}

		// Find maximum vector size
		size_t maxSize = 0;
		for(const auto& kv : inputParamRefs) {
			maxSize = std::max(maxSize, kv.second->get().size());
		}
		if(maxSize == 0) { output = {0}; return; }

		// Evaluate for each array index
		vector<float> result(maxSize);
		std::map<std::string,float> vars;

		for(size_t i = 0; i < maxSize; i++) {
			vars.clear();

			// Assign inputs as scalars ($1, $2, etc.)
			for(const auto& kv : inputParamRefs) {
				int idx = kv.first;
				const auto& vec = kv.second->get();
				std::string key = "$" + ofToString(idx + 1);
				if(vec.size() == 1) {
					vars[key] = vec[0];  // broadcast scalar
				} else if(i < vec.size()) {
					vars[key] = vec[i];
				} else {
					vars[key] = 0.0f;    // pad with 0
				}
			}

			// Constants
			vars["pi"] = M_PI;
			vars["PI"] = M_PI;
			vars["e"]  = M_E;
			vars["E"]  = M_E;

			// Evaluate
			try {
				result[i] = evalRPN(rpn, vars);
			}
			catch(std::exception& e) {
				lastError = e.what();
				result[i] = 0.0f;
			}
		}

		output = result;
	}

	// ---------- Formula compiler ----------
	void rebuildEvaluator() {
		formulaValid = false;
		try {
			auto tokens = tokenize(formulaString.get());
			rpn = shuntingYard(tokens);
			formulaValid = true;
		}
		catch(std::exception& e) {
			lastError = e.what();
		}
	}

	// Tokenizer
	std::vector<Token> tokenize(const std::string& str) {
		std::vector<Token> tokens;
		size_t i = 0;

		auto skipSpace = [&](){ while(i<str.size() && std::isspace(str[i])) i++; };
		auto peek = [&](size_t offset=0)->char{ return (i+offset<str.size()) ? str[i+offset] : '\0'; };
		auto consume = [&](){ return str[i++]; };

		while(i < str.size()) {
			skipSpace();
			if(i >= str.size()) break;

			char c = peek();

			// Number
			if(std::isdigit(c) || (c=='.' && std::isdigit(peek(1)))) {
				std::string num;
				while(std::isdigit(peek()) || peek()=='.') num += consume();
				Token t; t.type = Token::Number; t.text = num; t.value = std::stof(num);
				tokens.push_back(t);
				continue;
			}

			// Identifier (variables, functions)
			if(std::isalpha(c) || c=='_' || c=='$') {
				std::string id;
				while(std::isalnum(peek()) || peek()=='_' || peek()=='$') id += consume();
				Token t; t.type = Token::Identifier; t.text = id;
				tokens.push_back(t);
				continue;
			}

			// Operators and punctuation
			if(c=='(' || c==')' || c==',') {
				Token t;
				t.text = std::string(1, consume());
				if(c=='(') t.type = Token::LParen;
				else if(c==')') t.type = Token::RParen;
				else t.type = Token::Comma;
				tokens.push_back(t);
				continue;
			}

			// Check two-character operators first
			std::string op;
			if((c=='<' && peek(1)=='=') || (c=='>' && peek(1)=='=') ||
			   (c=='=' && peek(1)=='=') || (c=='!' && peek(1)=='=') ||
			   (c=='&' && peek(1)=='&') || (c=='|' && peek(1)=='|'))
			{
				op = std::string(1,consume()) + std::string(1,consume());
			}
			else if(c=='+' || c=='-' || c=='*' || c=='/' || c=='%' || c=='^' ||
					c=='<' || c=='>' || c=='!')
			{
				op = std::string(1, consume());
			}
			else {
				throw std::runtime_error("Unknown character: " + std::string(1,c));
			}

			Token t; t.type = Token::Operator; t.text = op;

			// Set operator properties
			auto setOp = [&](int prec, bool rAssoc=false){
				t.precedence = prec; t.rightAssoc = rAssoc;
			};

			if(op=="+") setOp(20);
			else if(op=="-") setOp(20);
			else if(op=="*") setOp(30);
			else if(op=="/") setOp(30);
			else if(op=="%") setOp(30);
			else if(op=="^") setOp(40, true);
			else if(op=="<" || op==">" || op=="<=" || op==">=") setOp(15);
			else if(op=="==" || op=="!=") setOp(14);
			else if(op=="&&") setOp(12);
			else if(op=="||") setOp(11);
			else if(op=="!") { setOp(35, true); t.unary = true; }

			// Check for unary - or +
			if((op=="-" || op=="+") && (tokens.empty() ||
			   tokens.back().type == Token::Operator ||
			   tokens.back().type == Token::LParen ||
			   tokens.back().type == Token::Comma))
			{
				t.unary = true;
				t.precedence = 35;
				t.rightAssoc = true;
			}

			tokens.push_back(t);
		}

		return tokens;
	}

	// Helper function
	bool isFunction(const std::string& name) {
		static const std::set<std::string> funcs = {
			"sin","cos","tan","asin","acos","atan","atan2",
			"sinh","cosh","tanh",
			"exp","log","log10","sqrt","abs",
			"floor","ceil","round",
			"min","max","clamp","step","smoothstep","pow","if"
		};
		return funcs.count(name) > 0;
	}

	// Shunting-yard algorithm
	RPN shuntingYard(const std::vector<Token>& tokens) {
		RPN out;
		std::vector<Token> opStack;

		// Function call tracking
		std::vector<std::string> funcStack;
		std::vector<int> argCountStack;

		// Tracking whether '(' belongs to a function call vs regular grouping
		std::vector<bool> lparenIsFunc;
		bool nextLParenIsFunc = false;

		for(size_t i=0;i<tokens.size();++i){
			const Token& t = tokens[i];
			switch(t.type){
			case Token::Number:
				out.push_back(t);
				break;

			case Token::Identifier: {
				// Function call if next token is '(' and the name is known
				bool call = (i+1<tokens.size() && tokens[i+1].type==Token::LParen && isFunction(t.text));
				if(call){
					funcStack.push_back(t.text);
					argCountStack.push_back(0);
					nextLParenIsFunc = true;       // ← mark that the upcoming '(' is a function paren
				} else {
					out.push_back(t);
				}
			} break;

			case Token::Comma: {
				// Comma must be inside a function argument list: pop ops to '('
				while(!opStack.empty() && opStack.back().type!=Token::LParen){
					out.push_back(opStack.back()); opStack.pop_back();
				}
				if(argCountStack.empty()) throw std::runtime_error("Misplaced comma");
				argCountStack.back() += 1;         // next argument
			} break;

			case Token::Operator: {
				while(!opStack.empty()){
					const Token& o2 = opStack.back();
					if(o2.type!=Token::Operator) break;
					bool higher = (!t.rightAssoc && t.precedence <= o2.precedence) ||
								   ( t.rightAssoc && t.precedence <  o2.precedence);
					if(higher){ out.push_back(o2); opStack.pop_back(); }
					else break;
				}
				opStack.push_back(t);
			} break;

			case Token::LParen:
				opStack.push_back(t);
				// record whether THIS '(' started a function call
				lparenIsFunc.push_back(nextLParenIsFunc);
				nextLParenIsFunc = false;
				break;

			case Token::RParen: {
				// Pop until matching '('
				while(!opStack.empty() && opStack.back().type!=Token::LParen){
					out.push_back(opStack.back()); opStack.pop_back();
				}
				if(opStack.empty()) throw std::runtime_error("Unbalanced parentheses");
				opStack.pop_back(); // pop '('

				// Check if that '(' belonged to a function
				if(lparenIsFunc.empty()) throw std::runtime_error("Internal paren state error");
				bool wasFunc = lparenIsFunc.back(); lparenIsFunc.pop_back();

				if(wasFunc){
					if(funcStack.empty() || argCountStack.empty())
						throw std::runtime_error("Function call state error");

					std::string fname = funcStack.back(); funcStack.pop_back();
					int argc = argCountStack.back(); argCountStack.pop_back();
					argc = argc + 1; // commas = N → args = N+1

					Token f; f.type=Token::Identifier; f.text=fname; f.value=float(argc);
					out.push_back(f);
				}
			} break;
			}
		}

		while(!opStack.empty()){
			if(opStack.back().type==Token::LParen || opStack.back().type==Token::RParen)
				throw std::runtime_error("Unbalanced parentheses");
			out.push_back(opStack.back()); opStack.pop_back();
		}

		if(!funcStack.empty()) throw std::runtime_error("Function call not closed");
		if(!lparenIsFunc.empty()) throw std::runtime_error("Internal paren state leak");

		return out;
	}


	// ---------- RPN evaluator ----------
	float evalRPN(const RPN& code, const std::map<std::string,float>& vars){
		std::vector<float> st;

		auto pop1 = [&](){ if(st.empty()) throw std::runtime_error("Stack underflow"); float a=st.back(); st.pop_back(); return a; };
		auto pop2 = [&](){ float b=pop1(); float a=pop1(); return std::pair<float,float>(a,b); };
		auto popN = [&](int n){ std::vector<float> args(n); for(int i=n-1;i>=0;--i) args[i]=pop1(); return args; };

		auto getIdent = [&](const std::string& id)->float{
			auto it = vars.find(id);
			if(it!=vars.end()) return it->second;
			// bare identifiers not in vars? allow constants pi/e handled earlier; everything else error
			throw std::runtime_error("Unknown identifier: " + id);
		};

		auto clampf = [](float x, float lo, float hi){
			return std::max(lo, std::min(hi, x));
		};

		for(const auto& t : code){
			if(t.type==Token::Number){
				st.push_back(t.value);
			} else if(t.type==Token::Identifier){
				// Function calls carry argc in value; plain identifiers should be looked up
				// Heuristic: functions we support are known by name; if not one of them -> variable
				const std::string& id = t.text;

				// Recognize functions:
				auto is = [&](const char* s){ return id==s; };
				if(is("sin")||is("cos")||is("tan")||is("asin")||is("acos")||is("atan")||
				   is("sinh")||is("cosh")||is("tanh")||is("exp")||is("log")||is("log10")||
				   is("sqrt")||is("abs")||is("floor")||is("ceil")||is("round")||
				   is("min")||is("max")||is("clamp")||is("step")||is("smoothstep")||
				   is("atan2")||is("pow")||is("if"))
				{
					int argc = int(t.value);
					if(argc > 0) {
						if(is("sin"))  { auto a=pop1(); st.push_back(std::sin(a));  continue; }
						if(is("cos"))  { auto a=pop1(); st.push_back(std::cos(a));  continue; }
						if(is("tan"))  { auto a=pop1(); st.push_back(std::tan(a));  continue; }
						if(is("asin")) { auto a=pop1(); st.push_back(std::asin(a)); continue; }
						if(is("acos")) { auto a=pop1(); st.push_back(std::acos(a)); continue; }
						if(is("atan")) { auto a=pop1(); st.push_back(std::atan(a)); continue; }
						
						if(is("atan2")){ auto [y,x]=pop2(); st.push_back(std::atan2(y,x)); continue; }
						
						if(is("sinh")) { auto a=pop1(); st.push_back(std::sinh(a)); continue; }
						if(is("cosh")) { auto a=pop1(); st.push_back(std::cosh(a)); continue; }
						if(is("tanh")) { auto a=pop1(); st.push_back(std::tanh(a)); continue; }
						
						if(is("exp"))  { auto a=pop1(); st.push_back(std::exp(a));  continue; }
						if(is("log"))  { auto a=pop1(); st.push_back(std::log(a));  continue; }
						if(is("log10")){ auto a=pop1(); st.push_back(std::log10(a));continue; }
						if(is("sqrt")) { auto a=pop1(); st.push_back(std::sqrt(a)); continue; }
						if(is("abs"))  { auto a=pop1(); st.push_back(std::fabs(a)); continue; }
						if(is("floor")){ auto a=pop1(); st.push_back(std::floor(a));continue; }
						if(is("ceil")) { auto a=pop1(); st.push_back(std::ceil(a)); continue; }
						if(is("round")){ auto a=pop1(); st.push_back(std::round(a));continue; }
						
						if(is("min")){
							if(argc<1) throw std::runtime_error("min() needs at least 1 arg");
							auto args = popN(argc);
							float m = args[0]; for(int i=1;i<argc;i++) m = std::min(m, args[i]);
							st.push_back(m); continue;
						}
						if(is("max")){
							if(argc<1) throw std::runtime_error("max() needs at least 1 arg");
							auto args = popN(argc);
							float m = args[0]; for(int i=1;i<argc;i++) m = std::max(m, args[i]);
							st.push_back(m); continue;
						}
						if(is("clamp")){
							if(argc!=3) throw std::runtime_error("clamp(x,lo,hi) needs 3 args");
							auto args = popN(3);
							st.push_back(clampf(args[0], args[1], args[2])); continue;
						}
						if(is("step")){
							if(argc!=2) throw std::runtime_error("step(edge,x) needs 2 args");
							auto args = popN(2);
							st.push_back(args[1] < args[0] ? 0.0f : 1.0f); continue;
						}
						if(is("smoothstep")){
							if(argc!=3) throw std::runtime_error("smoothstep(e0,e1,x) needs 3 args");
							auto args = popN(3);
							float e0=args[0], e1=args[1], x=args[2];
							if(e0==e1){ st.push_back(x<e0?0.0f:1.0f); continue; }
							float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
							st.push_back(t*t*(3.0f - 2.0f*t)); continue;
						}
						if(is("pow")){
							if(argc!=2) throw std::runtime_error("pow(a,b) needs 2 args");
							auto [a,b] = pop2();
							st.push_back(std::pow(a,b)); continue;
						}
						if(is("if")){
							if(argc!=3) throw std::runtime_error("if(cond,a,b) needs 3 args");
							auto args = popN(3);
							float cond = args[0];
							st.push_back( cond!=0.0f ? args[1] : args[2] );
							continue;
						}
					}

				}

				// Otherwise: variable/constant identifier
				st.push_back(getIdent(id));

			} else if(t.type==Token::Operator){
				const std::string& op = t.text;
				if(t.unary && t.text=="-"){ float a=pop1(); st.push_back(-a); continue; }
				if(t.unary && t.text=="!"){ float a=pop1(); st.push_back( a==0.0f ? 1.0f : 0.0f ); continue;
				}

				if(op=="+"){ auto [a,b]=pop2(); st.push_back(a+b); continue; }
				if(op=="-"){ auto [a,b]=pop2(); st.push_back(a-b); continue; }
				if(op=="*"){ auto [a,b]=pop2(); st.push_back(a*b); continue; }
				if(op=="/"){ auto [a,b]=pop2(); st.push_back(a/b); continue; }
				if(op=="%"){ auto [a,b]=pop2(); st.push_back(std::fmod(a,b)); continue; }
				if(op=="^"){ auto [a,b]=pop2(); st.push_back(std::pow(a,b)); continue; }
				// relational → 1.0 or 0.0
				if(op=="<"){  auto [a,b]=pop2(); st.push_back(a<b  ? 1.0f:0.0f); continue; }
				if(op==">"){  auto [a,b]=pop2(); st.push_back(a>b  ? 1.0f:0.0f); continue; }
				if(op=="<="){ auto [a,b]=pop2(); st.push_back(a<=b ? 1.0f:0.0f); continue; }
				if(op==">="){ auto [a,b]=pop2(); st.push_back(a>=b ? 1.0f:0.0f); continue; }
				if(op=="=="){ auto [a,b]=pop2(); st.push_back(a==b ? 1.0f:0.0f); continue; }
				if(op=="!="){ auto [a,b]=pop2(); st.push_back(a!=b ? 1.0f:0.0f); continue; }

				// logical (truthy: nonzero)
				if(op=="&&"){ auto [a,b]=pop2(); st.push_back( (a!=0.0f && b!=0.0f) ? 1.0f:0.0f ); continue; }
				if(op=="||"){ auto [a,b]=pop2(); st.push_back( (a!=0.0f || b!=0.0f) ? 1.0f:0.0f ); continue; }
				
				throw std::runtime_error("Unknown operator in RPN: " + op);
			} else {
				// LParen/RParen/Comma shouldn't appear in RPN
				throw std::runtime_error("Internal error: unexpected token in RPN");
			}
		}

		if(st.size()!=1) throw std::runtime_error("Evaluation ended with bad stack size");
		return st.back();
	}
};

#endif /* formula_h */
