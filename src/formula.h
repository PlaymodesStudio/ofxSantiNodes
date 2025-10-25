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

	void update(ofEventArgs &args) override {
		if(numInputs.get() != previousNumInputs) {
			updateInputs();
			previousNumInputs = numInputs.get();
		}

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

		// Determine broadcasting size
		size_t outputSize = 1;
		std::vector<bool> isScalar;
		isScalar.reserve(numInputs.get());

		for(int i = 0; i < numInputs.get(); i++) {
			auto it = inputParamRefs.find(i);
			if(it == inputParamRefs.end()) continue;
			size_t sz = it->second->get().size();
			bool scalar = (sz <= 1);
			isScalar.push_back(scalar);
			if(!scalar && sz > outputSize) outputSize = sz;
		}

		bool allScalars = true;
		for(bool s : isScalar) if(!s) { allScalars = false; break; }
		if(allScalars) outputSize = 1;

		std::vector<float> result;
		result.reserve(outputSize);

		for(size_t idx = 0; idx < outputSize; idx++) {
			float v = evaluateOnce(idx, isScalar);
			result.push_back(v);
		}

		output = result;
	}

	float evaluateOnce(size_t index, const std::vector<bool>& isScalar) {
		// Build a small environment for this index: variables & constants & functions
		std::map<std::string, float> vars = {
			{"pi", float(M_PI)}, {"e", float(M_E)}
		};

		// Inject $1..$N by value for this index
		for(int i = 0; i < numInputs.get(); i++) {
			auto it = inputParamRefs.find(i);
			if(it == inputParamRefs.end()) continue;

			const auto& vec = it->second->get();
			float val = 0.0f;
			if(vec.empty()) val = 0.0f;
			else if(i < (int)isScalar.size() && isScalar[i]) val = vec.front();
			else {
				if(index < vec.size()) val = vec[index];
				else val = vec.back();
			}
			vars["$" + ofToString(i+1)] = val;
		}

		try {
			return evalRPN(rpn, vars);
		} catch(const std::exception& e) {
			ofLogError("Formula") << "Eval error: " << e.what();
			return 0.0f;
		}
	}

	// ---------- Compilation pipeline ----------
	void rebuildEvaluator() {
		lastError.clear();
		formulaValid = false;
		rpn.clear();

		std::string src = formulaString.get();
		if(src.empty()) {
			lastError = "Empty formula";
			ofLogError("Formula") << lastError;
			return;
		}

		try {
			auto tokens = tokenize(src);
			rpn = shuntingYard(tokens);
			formulaValid = true;
		} catch(const std::exception& e) {
			lastError = e.what();
			ofLogError("Formula") << "Parse error: " << lastError;
		}
	}

	// ---------- Tokenizer ----------
	static bool isIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_' || c=='$'; }
	static bool isIdentChar (char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='$'; }

	std::vector<Token> tokenize(const std::string& s) {
		std::vector<Token> out;
		size_t i = 0;

		auto pushOp = [&](const std::string& op, bool unary)->void{
			Token t; t.type = Token::Operator; t.text = op; t.unary = unary;

			// Highest: unary !, unary -
			if(unary && (op=="-" || op=="!")) { t.precedence = 6; t.rightAssoc = true; }
			else if(op=="^")                 { t.precedence = 5; t.rightAssoc = true; }
			else if(op=="*"||op=="/"||op=="%"){ t.precedence = 4; }
			else if(op=="+"||op=="-")        { t.precedence = 3; }
			else if(op=="<"||op==">"||op=="<="||op==">="){ t.precedence = 2; }
			else if(op=="=="||op=="!=")      { t.precedence = 2; } // same as relational
			else if(op=="&&")                { t.precedence = 1; }
			else if(op=="||")                { t.precedence = 0; }
			else throw std::runtime_error("Unknown operator: " + op);

			out.push_back(t);
		};


		Token::Type prev = Token::LParen; // pretend lparen so leading '-' becomes unary

		while(i < s.size()){
			char c = s[i];

			if(std::isspace((unsigned char)c)){ i++; continue; }

			// numbers
			if(std::isdigit((unsigned char)c) || (c=='.' && i+1<s.size() && std::isdigit((unsigned char)s[i+1]))){
				size_t start = i;
				while(i<s.size() && (std::isdigit((unsigned char)s[i]) || s[i]=='.' || s[i]=='e' || s[i]=='E' || s[i]=='+' || s[i]=='-')){
					// rudimentary float scan; stop at first illegal sequence
					if((s[i]=='+'||s[i]=='-') && i>start){
						char prevc = s[i-1];
						if(prevc!='e' && prevc!='E') break;
					}
					i++;
				}
				Token t; t.type=Token::Number; t.text=s.substr(start, i-start); t.value = std::stof(t.text);
				out.push_back(t);
				prev = Token::Number;
				continue;
			}

			// identifiers / variables ($1, $foo, sin, clamp)
			if(isIdentStart(c)){
				size_t start = i++;
				while(i<s.size() && isIdentChar(s[i])) i++;
				Token t; t.type=Token::Identifier; t.text=s.substr(start, i-start);
				out.push_back(t);
				prev = Token::Identifier;
				continue;
			}

			// parentheses / comma
			if(c=='('){ out.push_back(Token{Token::LParen,"(",0}); i++; prev = Token::LParen; continue; }
			if(c==')'){ out.push_back(Token{Token::RParen,")",0}); i++; prev = Token::RParen; continue; }
			if(c==','){ out.push_back(Token{Token::Comma, ",",0}); i++; prev = Token::Comma;  continue; }

			// two-char operators: <= >= == != && ||
			if(i+1 < s.size()){
				std::string two = s.substr(i,2);
				static const std::set<std::string> twoOps = {"<=", ">=", "==", "!=", "&&", "||"};
				if(twoOps.count(two)){
					pushOp(two, /*unary=*/false);
					i += 2; prev = Token::Operator; continue;
				}
			}

			// single-char operators (including '!' and comparisons)
			if(c=='+' || c=='-' || c=='*' || c=='/' || c=='^' || c=='%' ||
			   c=='<' || c=='>' || c=='!' )
			{
				bool unary = false;
				if(c=='-'){
					unary = (prev==Token::Operator || prev==Token::LParen || prev==Token::Comma);
				} else if(c=='!'){
					// logical NOT is unary when not followed by '='
					unary = !(i+1 < s.size() && s[i+1]=='=');
				}
				pushOp(std::string(1,c), unary);
				i++; prev = Token::Operator; continue;
			}


			throw std::runtime_error(std::string("Unexpected character: '") + c + "'");
		}

		return out;
	}

	// ---------- Shunting-yard to RPN ----------
	RPN shuntingYard(const std::vector<Token>& tokens) {
		RPN out;
		std::vector<Token> opStack;
		std::vector<std::string> funcStack;   // function names
		std::vector<int> argCountStack;       // arg counts for current function

		// NEW: track whether each '(' corresponds to a function call
		std::vector<bool> lparenIsFunc;
		bool nextLParenIsFunc = false;

		auto isFunction = [&](const std::string& id)->bool{
			static const std::map<std::string,int> known = {
				{"sin",1},{"cos",1},{"tan",1},{"asin",1},{"acos",1},{"atan",1},
				{"atan2",2},{"sinh",1},{"cosh",1},{"tanh",1},
				{"exp",1},{"log",1},{"log10",1},{"sqrt",1},{"abs",1},
				{"floor",1},{"ceil",1},{"round",1},
				{"min",-1},{"max",-1},{"clamp",3},{"step",2},{"smoothstep",3},
				{"pow",2},{"if",3}
			};
			return known.count(id) > 0;
		};

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
