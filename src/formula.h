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
#include <numeric>

class formula : public ofxOceanodeNodeModel {
public:
	formula() : ofxOceanodeNodeModel("Formula") {}

	void setup() override {
		description =
			"Math formula evaluator with vector support. Edit the formula on the node. "
			"Use $1, $2, $3... for inputs. Supports +,-,*,/,%,^,( ), sin/cos/tan, atan2, "
			"sqrt, abs, pow, exp, log, min/max, clamp, step, smoothstep, floor/ceil/round. "
			"Vector functions: len(v), indices(v), at(v,i), sum(v), mean(v), min(v), max(v), "
			"median(v), rms(v), std(v), var(v), idxmin(v), idxmax(v), vec(...), repeat(x,n), concat(...). "
			"Vector literals: [1,2,3]. Scalars broadcast over vectors. Conditional: if(cond,a,b).";

		// Inspector-only params
		addInspectorParameter(numInputs.set("Num Inputs", 2, 1, 16));
		numInputsListener = numInputs.newListener([this](int &i){
			int current = (int)inputParameters.size();
			if(i > current){
				for(int k = current; k < i; ++k) addInputParameter(k);
			}else if(i < current){
				for(int k = current - 1; k >= i; --k) removeInputParameter(k);
			}
			previousNumInputs = i;
		});

		// Formula (moved to inspector only)
		addInspectorParameter(formulaString.set("Formula", "($1 + $2) / 2"));

		// Output
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

		// Multiline editor controls (inspector)
		addInspectorParameter(editorLines.set("Editor Lines", 3, 1, 40));
		addInspectorParameter(editorFontSize.set("Editor Font Size", 28.0f, 14.0f, 28.0f));

		// Preload editable buffer & listener
		formulaBuf = formulaString.get();
		formulaStrListener = formulaString.newListener([this](std::string &s){
			if(s != formulaBuf) formulaBuf = s;
		});

		// In-node custom editor (fixed width; height via Editor Lines)
		addCustomRegion(formulaEditorRegion, [this](){
			const float PADDING = 6.0f;

			// Height in pixels by line count
			const float baseLineHeight = ImGui::GetTextLineHeightWithSpacing();
			const int   lines          = editorLines.get();
			const float boxH           = lines * baseLineHeight;

			// Fixed content width to avoid spilling
			const float boxW = 240.0f;

			ImGui::BeginChild("FormulaEditor",
							  ImVec2(boxW, boxH + 2*PADDING),
							  true,
							  ImGuiWindowFlags_None);

			// Font scale (relative to ~14 px default)
			const float basePx = 14.0f;
			const float scale  = editorFontSize.get() / basePx;
			ImGui::SetWindowFontScale(scale);

			ImGui::SetCursorPos(ImVec2(PADDING, PADDING));

			static std::vector<char> buf;
			buf.assign(formulaBuf.begin(), formulaBuf.end());
			buf.push_back('\0');

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

			ImGui::SetWindowFontScale(1.0f);
			ImGui::EndChild();
		});

		formulaBuf = formulaString.get();
		formulaString.addListener(this, &formula::onFormulaParamChanged);

		// Initialize
		updateInputs();
		rebuildEvaluator();
	}

	// Ensure inputs exist before connections load
	void loadBeforeConnections(ofJson &json) override {
		deserializeParameter(json, numInputs);
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
	// ===== Parameters & UI =====
	ofParameter<int> numInputs;
	ofParameter<std::string> formulaString;
	ofParameter<std::vector<float>> output;

	customGuiRegion formulaEditorRegion;
	mutable std::string formulaBuf;
	ofEventListener formulaStrListener;
	ofParameter<int>   editorLines;
	ofParameter<float> editorFontSize;
	ofEventListener    numInputsListener;

	// Change tracking
	int    previousNumInputs = -1;
	std::string previousFormula = "";

	// Dynamic inputs
	std::map<int, std::shared_ptr<ofxOceanodeParameter<std::vector<float>>>> inputParameters;
	std::map<int, std::shared_ptr<ofParameter<std::vector<float>>>>          inputParamRefs;
	std::map<int, ofEventListener>                                           inputListeners;

	static inline bool isConstantVector(const std::vector<float>& v, float eps = 1e-6f){
		if(v.empty()) return true;
		float a0 = v[0];
		for(size_t i = 1; i < v.size(); ++i){
			if(std::fabs(v[i] - a0) > eps) return false;
		}
		return true;
	}

	// ===== Tokenizer / Parser Structures =====
	struct Token {
		enum Type { Number, Identifier, Operator, LParen, RParen, Comma, LBracket, RBracket } type;
		std::string text;
		float value = 0.0f;   // number literals or argc for identifiers-as-functions
		int precedence = 0;
		bool rightAssoc = false;
		bool unary = false;
	};
	using RPN = std::vector<Token>;

	RPN rpn;
	bool formulaValid = false;
	std::string lastError;

	// ===== Vector-aware Value + helpers =====
	struct Value {
		bool isVec = false;
		float f = 0.0f;
		std::vector<float> v;

		static Value scalar(float x){ Value r; r.f=x; r.isVec=false; return r; }
		static Value vector(std::vector<float> vv){ Value r; r.v=std::move(vv); r.isVec=true; return r; }

		size_t size() const { return isVec ? v.size() : 1; }
		float  getScalar() const { return isVec ? (v.empty()?0.0f:v[0]) : f; }

		std::vector<float> broadcast(size_t n) const {
			if(!isVec) return std::vector<float>(n, f);
			if(v.size()==n) return v;
			std::vector<float> out(n, 0.0f);
			for(size_t i=0;i<n;i++) out[i] = (i < v.size()) ? v[i] : v.back();
			return out;
		}
		float atClamped(int i) const {
			if(!isVec) return f;
			if(v.empty()) return 0.0f;
			if(i<0) return v.front();
			if((size_t)i>=v.size()) return v.back();
			return v[(size_t)i];
		}
	};
	using Env = std::map<std::string, Value>;

	static inline Value liftUnary(const Value& a, const std::function<float(float)>& f){
		if(!a.isVec) return Value::scalar(f(a.f));
		std::vector<float> out; out.reserve(a.v.size());
		for(float x: a.v) out.push_back(f(x));
		return Value::vector(std::move(out));
	}
	static inline Value liftBinary(const Value& a, const Value& b, const std::function<float(float,float)>& f){
		size_t n = std::max(a.size(), b.size());
		if(n==1) return Value::scalar(f(a.getScalar(), b.getScalar()));
		auto A = a.broadcast(n), B = b.broadcast(n);
		std::vector<float> out(n);
		for(size_t i=0;i<n;i++) out[i] = f(A[i], B[i]);
		return Value::vector(std::move(out));
	}

	// ===== Listeners / inputs =====
	void onFormulaParamChanged(std::string &s){
		if(s != formulaBuf) formulaBuf = s;
	}

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

		auto paramRef = std::make_shared<ofParameter<std::vector<float>>>();
		paramRef->set(name, {0}, {-FLT_MAX}, {FLT_MAX});
		inputParamRefs[index] = paramRef;

		auto oceaParam = addParameter(*paramRef);
		inputParameters[index] = oceaParam;

		inputListeners[index] = paramRef->newListener([this](std::vector<float>&){ calculate(); });
	}

	void removeInputParameter(int index) {
		std::string name = "$" + ofToString(index + 1);
		inputListeners.erase(index);
		removeParameter(name);
		inputParameters.erase(index);
		inputParamRefs.erase(index);
	}

	// ===== Public evaluation =====
	void calculate() {
		if(!formulaValid) {
			output = {0.0f};
			return;
		}

		// Determine max length for reference (also exposed as N)
		size_t N = 1;
		for(const auto& kv : inputParamRefs) {
			N = std::max(N, kv.second->get().size());
		}

		// Build evaluation environment with full vectors/scalars
		Env env;
		for(const auto& kv : inputParamRefs){
			int idx = kv.first;
			const auto& vec = kv.second->get();
			std::string key = "$" + ofToString(idx + 1);
			if(vec.size() <= 1) {
				env[key] = (vec.empty() ? Value::scalar(0.0f) : Value::scalar(vec[0]));
			} else {
				env[key] = Value::vector(vec);
			}
		}
		env["pi"] = Value::scalar(float(M_PI)); env["PI"] = env["pi"];
		env["e"]  = Value::scalar(float(M_E));  env["E"]  = env["e"];
		env["N"]  = Value::scalar(float(N));    // optional helper

		try{
			Value res = evalRPN(rpn, env);

			if(res.isVec){
				// Return the vector result as-is (if empty, emit [0])
				output = res.v.empty() ? std::vector<float>{0.0f} : res.v;
			}else{
				// Scalar result -> emit scalar (size 1)
				output = std::vector<float>{ res.f };
			}
		}
		catch(const std::exception& e){
			ofLogError("Formula") << "Eval error: " << e.what();
			output = {0.0f};
		}
	}


	// ===== Compiler pipeline =====
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

	// ===== Tokenizer =====
	static bool isIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_' || c=='$'; }
	static bool isIdentChar (char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='$'; }

	std::vector<Token> tokenize(const std::string& s) {
		std::vector<Token> out;
		size_t i = 0;

		auto pushOp = [&](const std::string& op, bool unary)->void{
			Token t; t.type = Token::Operator; t.text = op; t.unary = unary;

			if(unary && (op=="-" || op=="!")) { t.precedence = 35; t.rightAssoc = true; }
			else if(op=="^")                 { t.precedence = 40; t.rightAssoc = true; }
			else if(op=="*"||op=="/"||op=="%"){ t.precedence = 30; }
			else if(op=="+"||op=="-")        { t.precedence = 20; }
			else if(op=="<"||op==">"||op=="<="||op==">="){ t.precedence = 15; }
			else if(op=="=="||op=="!=")      { t.precedence = 14; }
			else if(op=="&&")                { t.precedence = 12; }
			else if(op=="||")                { t.precedence = 11; }
			else throw std::runtime_error("Unknown operator: " + op);

			out.push_back(t);
		};

		Token::Type prev = Token::LParen; // pretend '(' so leading '-' becomes unary

		auto peek = [&](int offs=0)->char{ return (i+offs < s.size()? s[i+offs] : '\0'); };

		while(i < s.size()){
			char c = s[i];

			if(std::isspace((unsigned char)c)){ i++; continue; }

			// numbers (simple: digits + dot)
			if(std::isdigit((unsigned char)c) || (c=='.' && std::isdigit((unsigned char)peek(1)))){
				size_t start = i;
				while(i<s.size() && (std::isdigit((unsigned char)s[i]) || s[i]=='.')) i++;
				Token t; t.type=Token::Number; t.text=s.substr(start, i-start); t.value = std::stof(t.text);
				out.push_back(t);
				prev = Token::Number;
				continue;
			}

			// identifiers / variables ($1, sin, clamp)
			if(isIdentStart(c)){
				size_t start = i++;
				while(i<s.size() && isIdentChar(s[i])) i++;
				Token t; t.type=Token::Identifier; t.text=s.substr(start, i-start);
				out.push_back(t);
				prev = Token::Identifier;
				continue;
			}

			// parentheses / comma / brackets
			if(c=='('){ out.push_back(Token{Token::LParen,"(",0}); i++; prev = Token::LParen; continue; }
			if(c==')'){ out.push_back(Token{Token::RParen,")",0}); i++; prev = Token::RParen; continue; }
			if(c==','){ out.push_back(Token{Token::Comma, ",",0}); i++; prev = Token::Comma;  continue; }
			if(c=='['){ out.push_back(Token{Token::LBracket,"[",0}); i++; prev = Token::LBracket; continue; }
			if(c==']'){ out.push_back(Token{Token::RBracket,"]",0}); i++; prev = Token::RBracket; continue; }

			// two-char operators
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
					unary = (prev==Token::Operator || prev==Token::LParen || prev==Token::Comma || prev==Token::LBracket);
				} else if(c=='!'){
					unary = !(i+1 < s.size() && s[i+1]=='=');
				}
				pushOp(std::string(1,c), unary);
				i++; prev = Token::Operator; continue;
			}

			throw std::runtime_error(std::string("Unexpected character: '") + c + "'");
		}

		return out;
	}

	// ===== Function table =====
	bool isFunction(const std::string& name) {
		static const std::set<std::string> funcs = {
			"sin","cos","tan","asin","acos","atan","atan2",
			"sinh","cosh","tanh",
			"exp","log","log10","sqrt","abs",
			"floor","ceil","round",
			"min","max","clamp","step","smoothstep","pow","if",
			// Vector-aware
			"len","indices","at","sum","mean","median","rms","std","var","idxmin","idxmax",
			"vec","repeat","concat"
		};
		return funcs.count(name) > 0;
	}

	// ===== Shunting-yard → RPN (supports vector literals) =====
	RPN shuntingYard(const std::vector<Token>& tokens) {
		RPN out;
		std::vector<Token> opStack;

		// function call tracking
		std::vector<std::string> funcStack;
		std::vector<int> argCountStack;
		std::vector<bool> lparenIsFunc;
		bool nextLParenIsFunc = false;

		// vector literal tracking
		std::vector<int> vecArgCount;

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
					nextLParenIsFunc = true;
				} else {
					out.push_back(t);
				}
			} break;

			case Token::LBracket:
				opStack.push_back(t);
				vecArgCount.push_back(0);
				break;

			case Token::RBracket: {
				while(!opStack.empty() && opStack.back().type!=Token::LBracket){
					out.push_back(opStack.back()); opStack.pop_back();
				}
				if(opStack.empty()) throw std::runtime_error("Unbalanced brackets");
				opStack.pop_back(); // pop '['

				if(vecArgCount.empty()) throw std::runtime_error("Vector literal state error");
				{
					int argc = vecArgCount.back(); vecArgCount.pop_back();
					argc = argc + 1;
					Token lit; lit.type=Token::Identifier; lit.text="__veclit"; lit.value=float(argc);
					out.push_back(lit);
				}
			} break;

			case Token::Comma: {
				while(!opStack.empty() && opStack.back().type!=Token::LParen && opStack.back().type!=Token::LBracket){
					out.push_back(opStack.back()); opStack.pop_back();
				}
				if(opStack.empty()) throw std::runtime_error("Misplaced comma");
				if(opStack.back().type==Token::LParen){
					if(argCountStack.empty()) throw std::runtime_error("Function arg state error");
					argCountStack.back() += 1;
				}else{
					if(vecArgCount.empty()) throw std::runtime_error("Vector literal comma state error");
					vecArgCount.back() += 1;
				}
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
				lparenIsFunc.push_back(nextLParenIsFunc);
				nextLParenIsFunc = false;
				break;

			case Token::RParen: {
				while(!opStack.empty() && opStack.back().type!=Token::LParen){
					out.push_back(opStack.back()); opStack.pop_back();
				}
				if(opStack.empty()) throw std::runtime_error("Unbalanced parentheses");
				opStack.pop_back(); // pop '('

				if(lparenIsFunc.empty()) throw std::runtime_error("Internal paren state error");
				if(lparenIsFunc.back()){
					if(funcStack.empty() || argCountStack.empty()) throw std::runtime_error("Function call state error");
					std::string fname = funcStack.back(); funcStack.pop_back();
					int argc = argCountStack.back(); argCountStack.pop_back();
					argc = argc + 1;

					Token f; f.type=Token::Identifier; f.text=fname; f.value=float(argc);
					out.push_back(f);
				}
				lparenIsFunc.pop_back();
			} break;
			}
		}

		while(!opStack.empty()){
			if(opStack.back().type==Token::LParen || opStack.back().type==Token::RParen ||
			   opStack.back().type==Token::LBracket || opStack.back().type==Token::RBracket)
				throw std::runtime_error("Unbalanced parentheses or brackets");
			out.push_back(opStack.back()); opStack.pop_back();
		}

		if(!funcStack.empty()) throw std::runtime_error("Function call not closed");
		if(!lparenIsFunc.empty()) throw std::runtime_error("Internal paren state leak");
		if(!vecArgCount.empty()) throw std::runtime_error("Vector literal not closed");

		return out;
	}

	static inline bool truthy(const Value& x){
		if(x.isVec) {
			// treat any nonzero element as true (you could also require all true)
			for(float a : x.v) if(a != 0.0f) return true;
			return false;
		}
		return x.f != 0.0f;
	}
	static inline float sampleAt(const Value& x, size_t i){
		if(!x.isVec) return x.f;
		if(x.v.empty()) return 0.0f;
		if(i < x.v.size()) return x.v[i];
		return x.v.back(); // clamp-to-last
	}
	
	// ===== Vector-aware evaluator (returns Value) =====
	Value evalRPN(const RPN& code, const Env& env){
		std::vector<Value> st;

		auto pop1=[&](){ if(st.empty()) throw std::runtime_error("Stack underflow"); auto x=st.back(); st.pop_back(); return x; };
		auto pop2=[&](){ auto b=pop1(); auto a=pop1(); return std::pair<Value,Value>(a,b); };
		auto popN=[&](int n){ std::vector<Value> a(n); for(int i=n-1;i>=0;--i) a[i]=pop1(); return a; };

		auto getIdent=[&](const std::string& id)->Value{
			auto it=env.find(id);
			if(it!=env.end()) return it->second;
			throw std::runtime_error("Unknown identifier: "+id);
		};
		auto asInt = [](float x){ return (int)std::floor(x+1e-6f); };
		auto clampf=[](float x,float lo,float hi){ return std::max(lo,std::min(hi,x)); };

		// reducers for 1-arg functions → scalar
		auto reduce = [&](const Value& a, const std::string& which)->Value{
			const std::vector<float>& v = a.isVec ? a.v : std::vector<float>{a.f};
			if(v.empty()) return Value::scalar(0.0f);

			if(which=="sum"){
				double s=0; for(float x:v) s+=x;
				return Value::scalar((float)s);
			}
			if(which=="mean"){
				double s=0; for(float x:v) s+=x;
				return Value::scalar((float)(s/v.size()));
			}
			if(which=="median"){
				std::vector<float> t=v;
				std::nth_element(t.begin(), t.begin()+t.size()/2, t.end());
				float m = t[t.size()/2];
				if((t.size()%2)==0){
					std::nth_element(t.begin(), t.begin()+t.size()/2-1, t.end());
					m=(m+t[t.size()/2-1])*0.5f;
				}
				return Value::scalar(m);
			}
			if(which=="var" || which=="std"){
				if(v.size()<2) return Value::scalar(0.0f);
				double m=0; for(float x:v) m+=x; m/=v.size();
				double s=0; for(float x:v){ double d=x-m; s+=d*d; }
				float var = (float)(s/v.size());
				if(which=="var") return Value::scalar(var);
				return Value::scalar(std::sqrt(var)); // std
			}
			if(which=="rms"){
				double s=0; for(float x:v) s+=double(x)*x;
				return Value::scalar((float)std::sqrt(s/v.size()));
			}
			if(which=="idxmin"){
				int m=0; for(int i=1;i<(int)v.size();++i) if(v[i]<v[m]) m=i;
				return Value::scalar((float)m);
			}
			if(which=="idxmax"){
				int m=0; for(int i=1;i<(int)v.size();++i) if(v[i]>v[m]) m=i;
				return Value::scalar((float)m);
			}
			if(which=="min"){
				float m=v[0]; for(float x:v) m=std::min(m,x);
				return Value::scalar(m);
			}
			if(which=="max"){
				float m=v[0]; for(float x:v) m=std::max(m,x);
				return Value::scalar(m);
			}
			throw std::runtime_error("Unknown reducer: "+which);
		};

		for(const auto& t: code){
			if(t.type==Token::Number){ st.push_back(Value::scalar(t.value)); continue; }

			if(t.type==Token::Identifier){
				const std::string& id = t.text;

				// variable / constant?
				if(env.find(id)!=env.end()){ st.push_back(getIdent(id)); continue; }

				// vector literal
				if(id=="__veclit"){
					int argc=(int)t.value;
					auto args=popN(argc);
					std::vector<float> out; out.reserve(args.size());
					for(auto& a: args) out.push_back(a.getScalar());
					st.push_back(Value::vector(std::move(out)));
					continue;
				}

				// functions (argc encoded in value)
				int argc=(int)t.value;
				auto is=[&](const char* s){ return id==s; };

				// introspection / gather
				if(is("len")){ if(argc!=1) throw std::runtime_error("len(v) expects 1 arg"); st.push_back(Value::scalar((float)pop1().size())); continue; }
				if(is("indices")){
					if(argc!=1) throw std::runtime_error("indices(v) expects 1 arg");
					auto a=pop1(); size_t n=a.size(); std::vector<float> idx(n); for(size_t i=0;i<n;i++) idx[i]=(float)i;
					st.push_back(Value::vector(std::move(idx))); continue;
				}
				if(is("at")){
					if(argc!=2) throw std::runtime_error("at(v,i) expects 2 args");
					auto iVal = pop1(); auto vVal = pop1();
					if(!iVal.isVec) { st.push_back(Value::scalar(vVal.atClamped(asInt(iVal.getScalar())))); }
					else{ std::vector<float> out; out.reserve(iVal.v.size()); for(float ii: iVal.v) out.push_back(vVal.atClamped(asInt(ii))); st.push_back(Value::vector(std::move(out))); }
					continue;
				}

				// constructors
				if(is("vec")){
					auto args=popN(argc);
					std::vector<float> out;
					for(auto& a: args){ if(a.isVec) out.insert(out.end(), a.v.begin(), a.v.end()); else out.push_back(a.f); }
					st.push_back(Value::vector(std::move(out))); continue;
				}
				if(is("repeat")){
					if(argc!=2) throw std::runtime_error("repeat(x,n) expects 2 args");
					auto nVal=pop1(); auto xVal=pop1();
					int n = std::max(0, asInt(nVal.getScalar()));
					st.push_back(Value::vector(std::vector<float>(n, xVal.getScalar()))); continue;
				}
				if(is("concat")){
					auto args=popN(argc);
					std::vector<float> out;
					for(auto& a: args){ if(a.isVec) out.insert(out.end(), a.v.begin(), a.v.end()); else out.push_back(a.f); }
					st.push_back(Value::vector(std::move(out))); continue;
				}

				// reducers (1 arg → scalar)
				if(is("sum")||is("mean")||is("median")||is("rms")||is("std")||is("var")||is("idxmin")||is("idxmax")||is("min")||is("max")){
					if(argc==1){ st.push_back(reduce(pop1(), id)); continue; }
					// min/max with multiple args → element-wise with broadcasting
					if((id=="min"||id=="max") && argc>=2){
						auto args=popN(argc);
						auto f = (id=="min") ? [](float a,float b){return std::min(a,b);} : [](float a,float b){return std::max(a,b);};
						Value cur=args[0]; for(int i=1;i<argc;i++) cur = liftBinary(cur,args[i],f);
						st.push_back(cur); continue;
					}
					throw std::runtime_error(id+" expects 1 arg");
				}

				// scalar math (lifted)
				if(is("sin")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::sin(x);})); continue; }
				if(is("cos")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::cos(x);})); continue; }
				if(is("tan")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::tan(x);})); continue; }
				if(is("asin")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::asin(x);})); continue; }
				if(is("acos")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::acos(x);})); continue; }
				if(is("atan")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::atan(x);})); continue; }
				if(is("sinh")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::sinh(x);})); continue; }
				if(is("cosh")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::cosh(x);})); continue; }
				if(is("tanh")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::tanh(x);})); continue; }
				if(is("exp")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::exp(x);}));  continue; }
				if(is("log")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::log(x);}));  continue; }
				if(is("log10")){auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::log10(x);}));continue; }
				if(is("sqrt")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::sqrt(x);})); continue; }
				if(is("abs")){  auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::fabs(x);})); continue; }
				if(is("floor")){auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::floor(x);}));continue; }
				if(is("ceil")){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::ceil(x);}));  continue; }
				if(is("round")){auto a=pop1(); st.push_back(liftUnary(a,[](float x){return std::round(x);}));continue; }

				if(is("atan2")){ if(argc!=2) throw std::runtime_error("atan2(y,x) needs 2");
					auto x=pop1(), y=pop1(); st.push_back(liftBinary(y,x,[](float a,float b){return std::atan2(a,b);})); continue; }
				if(is("pow")){ if(argc!=2) throw std::runtime_error("pow(a,b) needs 2");
					auto b=pop1(), a=pop1(); st.push_back(liftBinary(a,b,[](float x,float y){return std::pow(x,y);})); continue; }
				if(is("clamp")){ if(argc!=3) throw std::runtime_error("clamp(x,lo,hi) needs 3");
					auto hi=pop1(), lo=pop1(), x=pop1();
					auto t = liftBinary(x, lo, [](float a,float b){return std::max(a,b);});
					st.push_back(liftBinary(t, hi, [](float a,float b){return std::min(a,b);})); continue; }
				if(is("step")){ if(argc!=2) throw std::runtime_error("step(edge,x) needs 2");
					auto x=pop1(), e=pop1(); st.push_back(liftBinary(e,x,[](float E,float X){return X<E?0.0f:1.0f;})); continue; }
				if(is("smoothstep")){ if(argc!=3) throw std::runtime_error("smoothstep(e0,e1,x) needs 3");
					auto x=pop1(), e1=pop1(), e0=pop1();
					auto t = liftBinary( liftBinary(x,e0,[](float X,float A){return X-A;}),
										 liftBinary(e1,e0,[](float B,float A){return B-A;}),
										 [&](float num,float den){ return den==0? (num<0?0.0f:1.0f) : clampf(num/den,0.0f,1.0f); });
					st.push_back(liftUnary(t, [](float t){ return t*t*(3.0f-2.0f*t); })); continue; }
				if(is("if")){
					if(argc != 3) throw std::runtime_error("if(cond,a,b) needs 3");
					// RPN pop order: last argument first
					Value elseV = pop1();  // b
					Value thenV = pop1();  // a
					Value cond  = pop1();  // cond

					// SCALAR condition: return branch AS-IS (no length alignment)
					if(!cond.isVec){
						const bool ctrue = (cond.getScalar() != 0.0f);
						st.push_back(ctrue ? thenV : elseV);
						continue;
					}

					// VECTOR condition: per-index selection, length = cond.size()
					const size_t L = cond.size();
					std::vector<float> out(L);
					for(size_t i = 0; i < L; ++i){
						const bool ctrue = (cond.v[i] != 0.0f);
						out[i] = ctrue ? sampleAt(thenV, i) : sampleAt(elseV, i);
					}
					st.push_back(Value::vector(std::move(out)));
					continue;
				}
				throw std::runtime_error("Unknown identifier: "+id);
			}

			if(t.type==Token::Operator){
				const std::string& op = t.text;
				if(t.unary && op=="-"){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return -x;})); continue; }
				if(t.unary && op=="!"){ auto a=pop1(); st.push_back(liftUnary(a,[](float x){return x==0.0f?1.0f:0.0f;})); continue; }
				if(op=="+"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x+y;})); continue; }
				if(op=="-"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x-y;})); continue; }
				if(op=="*"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x*y;})); continue; }
				if(op=="/"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x/y;})); continue; }
				if(op=="%"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return std::fmod(x,y);})); continue; }
				if(op=="^"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return std::pow(x,y);})); continue; }
				if(op=="<"){  auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x< y?1.0f:0.0f;})); continue; }
				if(op==">"){  auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x> y?1.0f:0.0f;})); continue; }
				if(op=="<="){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x<=y?1.0f:0.0f;})); continue; }
				if(op==">="){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x>=y?1.0f:0.0f;})); continue; }
				if(op=="=="){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x==y?1.0f:0.0f;})); continue; }
				if(op!="!=" && op!="&&" && op!="||"){ /* fallthrough later */ }
				if(op=="!="){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return x!=y?1.0f:0.0f;})); continue; }
				if(op=="&&"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return (x!=0.0f && y!=0.0f)?1.0f:0.0f;})); continue; }
				if(op=="||"){ auto [a,b]=pop2(); st.push_back(liftBinary(a,b,[](float x,float y){return (x!=0.0f || y!=0.0f)?1.0f:0.0f;})); continue; }
				throw std::runtime_error("Unknown operator: "+op);
			}
		}

		if(st.size()!=1) throw std::runtime_error("Evaluation ended with bad stack size");
		return st.back();
	}
};

#endif /* formula_h */
