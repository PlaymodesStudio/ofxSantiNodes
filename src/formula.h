#ifndef formula_h
#define formula_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
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
#include <fstream>
#include <future>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>

class formula : public ofxOceanodeNodeModel {
public:
	formula() : ofxOceanodeNodeModel("Formula") {
		// Use the same API-key locations as scDynGen's assistant.
		auto tryReadKey = [this](const std::string& path) {
			std::ifstream file(path);
			if(!file) return false;
			std::getline(file, llmApiKey);
			return !llmApiKey.empty();
		};

		const char* envKey = std::getenv("ANTHROPIC_API_KEY");
		if(envKey && *envKey) {
			llmApiKey = envKey;
		} else if(!tryReadKey(ofToDataPath("anthropic_api_key.txt"))) {
			tryReadKey(ofFilePath::getUserHomeDir() + "/.anthropic_api_key");
		}

		while(!llmApiKey.empty() && std::isspace(static_cast<unsigned char>(llmApiKey.back()))) {
			llmApiKey.pop_back();
		}
	}

	void setup() override {
		description =
			"Math formula evaluator with vector support. Edit the formula on the node. "
			"The built-in AI assistant can generate a new formula or repair the current one. "
			"Formula files can be loaded from or saved to the data/formulas library. "
			"Use $i1, $i2... for automatically-created inputs and assign $o1, $o2... for outputs. "
			"Legacy $1, $2... input names and a final bare-expression output remain supported. "
			"Rename ports with identifier labels such as $i1.label=frequency; labels can then be used as port aliases. "
			"Supports +,-,*,/,%,^,( ), sin/cos/tan, atan2, "
			"sqrt, abs, pow, exp, log, min/max, clamp, step, smoothstep, floor/ceil/round. "
			"Vector functions: len(v), indices(v), at(v,i), sum(v), mean(v), min(v), max(v), "
			"median(v), rms(v), std(v), var(v), idxmin(v), idxmax(v), vec(...), repeat(x,n), concat(...), sort(v). "
			"Vector literals: [1,2,3]. Scalars broadcast over vectors. Conditional: if(cond,a,b). "
			"Variables: separate statements with ';' and assign with '=', e.g. \"x = $i1 + $i2; $o1 = x * 100\".";

		// Formula (moved to inspector only)
		addInspectorParameter(formulaString.set("Formula", "($1 + $2) / 2"));

		// Formula library controls
		addInspectorParameter(formulaName.set("Formula Name", "formula"));
		addInspectorParameter(loadFormulaButton.set("Load Formula"));
		addInspectorParameter(saveFormulaButton.set("Save Formula"));
		loadFormulaListener = loadFormulaButton.newListener([this]() {
			loadFormulaFile();
		});
		saveFormulaListener = saveFormulaButton.newListener([this]() {
			saveFormulaFile();
		});

		// Multiline editor controls (inspector)
		addInspectorParameter(editorLines.set("Editor Lines", 6, 1, 40));
		addInspectorParameter(editorWidth.set("Editor Width", 240.0f, 160.0f, 800.0f));
		addInspectorParameter(editorFontSize.set("Editor Font Size", 14.0f, 8.0f, 48.0f));
		addInspectorParameter(editorWordWrap.set("Auto Line Wrap", true));

		// Preload editable buffer & listener
		formulaBuf = formulaString.get();
		formulaStrListener = formulaString.newListener([this](std::string &s){
			if(s != formulaBuf) formulaBuf = s;
		});
		formulaString.addListener(this, &formula::onFormulaParamChanged);

		// In-node custom editor (dimensions and text behavior are inspector-controlled)
		formulaEditorRegion.set("Formula Editor", [this](){
			float zoom = ofxOceanodeShared::getZoomLevel();
			const auto& customRegionContext = ofxOceanodeShared::getCustomRegionRenderContext();
			const float PADDING = 6.0f * zoom;
			const float parentFontPx = ImGui::GetFontSize();
			const float editorFontPx = parentFontPx * (editorFontSize.get() / 14.0f);
			const float toolbarH = parentFontPx + 8.0f * zoom;
			const float requestedEditorH = editorLines.get() *
				(editorFontPx + ImGui::GetStyle().ItemSpacing.y);

			const float boxW = customRegionContext.active
				? std::max(1.0f, customRegionContext.width)
				: editorWidth.get() * zoom;
			const float totalH = customRegionContext.active
				? std::max(1.0f, customRegionContext.height)
				: toolbarH + requestedEditorH + 3.0f * PADDING;
			const float editorH = customRegionContext.active
				? std::max(1.0f, totalH - toolbarH - 3.0f * PADDING)
				: requestedEditorH;

			ImGui::BeginChild("FormulaEditor",
							  ImVec2(boxW, totalH),
							  true,
							  ImGuiWindowFlags_None);

			// Child windows do not retain the canvas window's font scale. Restore the
			// actual inherited size before drawing the toolbar.
			const float childFontBase = std::max(1.0f, ImGui::GetFont()->LegacySize);
			ImGui::SetWindowFontScale(std::max(0.05f, parentFontPx / childFontBase));

			ImGui::SetCursorPos(ImVec2(PADDING, PADDING));
			bool busy = llmPending.load();
			ImGui::PushStyleColor(ImGuiCol_Button,
				busy ? IM_COL32(40, 20, 65, 255) : IM_COL32(60, 38, 90, 255));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 58, 130, 255));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(115, 78, 165, 255));
			if(ImGui::Button(busy ? "..." : "AI", ImVec2(38.0f * zoom, 0)) && !busy) {
				ImGui::OpenPopup("##formula_ai_popup");
			}
			ImGui::PopStyleColor(3);
			ImGui::SameLine(0, 6.0f * zoom);
			if(!formulaValid && !lastError.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", lastError.c_str());
			} else if(!llmStatusMsg.empty()) {
				ImGui::TextDisabled("%s", llmStatusMsg.c_str());
			} else {
				ImGui::TextDisabled("%d input%s / %d output%s",
					static_cast<int>(inputParameters.size()), inputParameters.size() == 1 ? "" : "s",
					static_cast<int>(outputParameters.size()), outputParameters.size() == 1 ? "" : "s");
			}

			static std::vector<char> buf;
			buf.assign(formulaBuf.begin(), formulaBuf.end());
			buf.push_back('\0');

			ImGui::SetCursorPos(ImVec2(PADDING, PADDING + toolbarH));
			ImVec2 inputSize(std::max(1.0f, boxW - 2.0f * PADDING), editorH);
			ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_AllowTabInput |
				ImGuiInputTextFlags_CallbackResize;
			if(editorWordWrap.get()) inputFlags |= ImGuiInputTextFlags_WordWrap;

			// Scale relative to Oceanode's actual active font rather than assuming
			// that the child window is using a 14 px font.
			ImGui::PushFont(nullptr, childFontBase * (editorFontSize.get() / 14.0f));

			bool changed = ImGui::InputTextMultiline(
				"##formulaML",
				buf.data(), buf.size(),
				inputSize,
				inputFlags,
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
			ImGui::PopFont();

			if(changed){
				llmStatusMsg.clear();
				formulaBuf.assign(buf.data(), std::strlen(buf.data()));
				if(formulaBuf != formulaString.get()){
					formulaString.set(formulaBuf);
				}
			}

			drawAIPopup();
			ImGui::EndChild();
		});
		addCustomRegion(formulaEditorRegion, formulaEditorRegion.get());

		// Register the editor before creating any ports. ofParameterGroup preserves
		// insertion order, so both initial and later dynamic ports stay together
		// below the editor instead of being split around it.
		rebuildEvaluator();

		// Initialize output values.
		calculate();
	}

	// Ensure inputs exist before connections load
	void loadBeforeConnections(ofJson &json) override {
		deserializeParameter(json, formulaString);
		formulaBuf = formulaString.get();
		previousFormula = formulaString.get();
		rebuildEvaluator();
	}

	void update(ofEventArgs &args) override {
		pollLLMResult();
		if(formulaString.get() != previousFormula) {
			previousFormula = formulaString.get();
			rebuildEvaluator();
			calculate();
		}
	}

private:
	// ===== Parameters & UI =====
	ofParameter<std::string> formulaString;
	ofParameter<std::string> formulaName;
	ofParameter<void> loadFormulaButton;
	ofParameter<void> saveFormulaButton;

	customGuiRegion formulaEditorRegion;
	mutable std::string formulaBuf;
	ofEventListener formulaStrListener;
	ofParameter<int>   editorLines;
	ofParameter<float> editorWidth;
	ofParameter<float> editorFontSize;
	ofParameter<bool>  editorWordWrap;
	ofEventListener    loadFormulaListener;
	ofEventListener    saveFormulaListener;

	// ===== AI formula assistant =====
	std::string              llmApiKey;
	std::future<std::string> llmFuture;
	std::atomic<bool>        llmPending { false };
	std::atomic<bool>        llmDone { false };
	bool                     llmHasError = false;
	bool                     llmFixMode = false;
	std::string              llmStatusMsg;
	char                     llmPromptBuf[2048] = {};

	// Change tracking
	std::string previousFormula = "";

	// Dynamic inputs
	std::map<int, std::shared_ptr<ofxOceanodeParameter<std::vector<float>>>> inputParameters;
	std::map<int, std::shared_ptr<ofParameter<std::vector<float>>>>          inputParamRefs;
	std::map<int, ofEventListener>                                           inputListeners;

	// Dynamic outputs. Key 0 is the legacy "Output" port; positive keys are
	// explicit $o1, $o2... ports.
	std::map<int, std::shared_ptr<ofxOceanodeParameter<std::vector<float>>>> outputParameters;
	std::map<int, std::shared_ptr<ofParameter<std::vector<float>>>>          outputParamRefs;

	// ===== AI assistant =====
	static std::string buildFormulaSystemPrompt() {
		return R"FORMULA_PROMPT(You generate expressions for the Formula node in Oceanode, a modular visual environment.

Return ONLY valid Formula-node source. Do not use Markdown, code fences, comments, section headings, or explanatory prose.

LANGUAGE RULES
- Inputs are $i1, $i2, $i3, etc. Referencing one automatically creates that input port.
- Legacy $1, $2, $3 input aliases are also supported and refer to the same numbered inputs.
- Outputs are assignments to $o1, $o2, $o3, etc. Assigning them automatically creates those output ports.
- Optional port labels are metadata statements: $i1.label=frequency; $o1.label=result. Labels must be identifiers without spaces, must be unique, and become aliases for their ports in expressions and assignments.
- Scalars automatically broadcast over vectors. When vectors differ in length, shorter values clamp to their last element.
- Constants: pi, PI, e, E, and N (the longest connected input-vector length).
- Operators: + - * / % ^, < > <= >= == !=, && ||, unary - and !.
- Math functions: sin, cos, tan, asin, acos, atan, atan2, sinh, cosh, tanh, exp, log, log10, sqrt, abs, floor, ceil, round, pow, min, max, clamp, step, smoothstep.
- Conditional: if(condition, value_when_true, value_when_false). Conditions may be vectors.
- Vector functions: len(v), indices(v), at(v,i), sum(v), mean(v), median(v), rms(v), std(v), var(v), idxmin(v), idxmax(v), vec(...), repeat(x,n), concat(...), sort(v), pairdist(x,y).
- Vector literals use brackets, for example [1, 2, 3].
- Intermediate variables are allowed. Separate statements with semicolons and assign with '='.
- Preferred output form: centered = $i1 - mean($i1); $o1 = centered * 2
- Multiple outputs: $o1 = $i1 + $i2; $o2 = $i1 - $i2
- Legacy formulas may omit $oN and use one final bare expression, which creates the old Output port.
- Do not mix explicit $oN outputs with a bare output expression.
- Identifiers contain letters, digits, underscores, or $. Do not assign to inputs or to pi, PI, e, E, or N.
- Numeric literals must be ordinary decimal notation; scientific notation is not supported.
- There are no comments, strings, loops, indexing brackets, ternary operators, or user-defined functions.

EXAMPLES
Average two inputs using label aliases:
$i1.label=left; $i2.label=right; $o1.label=average; average = (left + right) / 2

Normalize a vector safely:
lo = min($i1); hi = max($i1); $o1 = if(hi == lo, repeat(0, len($i1)), ($i1 - lo) / (hi - lo))

Select positive values:
$o1 = if($i1 > 0, $i1, 0)
)FORMULA_PROMPT";
	}

	static std::string shellQuote(const std::string& value) {
		std::string quoted = "'";
		for(char c : value) {
			if(c == '\'') quoted += "'\\''";
			else quoted.push_back(c);
		}
		quoted.push_back('\'');
		return quoted;
	}

	static std::string stripCodeFences(std::string result) {
		result = trimStr(result);
		if(result.rfind("```", 0) != 0) return result;

		size_t firstLineEnd = result.find('\n');
		if(firstLineEnd == std::string::npos) return result;
		result.erase(0, firstLineEnd + 1);
		size_t closingFence = result.rfind("```");
		if(closingFence != std::string::npos) result.erase(closingFence);
		return trimStr(result);
	}

	static std::string callAnthropicAPI(const std::string& fullPrompt,
										const std::string& systemPrompt,
										const std::string& apiKey) {
		try {
			ofJson requestBody = {
				{"model", "claude-sonnet-4-5"},
				{"max_tokens", 1024},
				{"system", systemPrompt},
				{"messages", ofJson::array({{{"role", "user"}, {"content", fullPrompt}}})}
			};

			static std::atomic<unsigned long long> requestCounter { 0 };
			const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
			const auto sequence = requestCounter.fetch_add(1);
			std::string tmpDir = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp";
			if(!tmpDir.empty() && tmpDir.back() != '/') tmpDir.push_back('/');
			const std::string tmpPath = tmpDir + "formula_api_req_" +
				std::to_string(ticks) + "_" + std::to_string(sequence) + ".json";

			{
				std::ofstream file(tmpPath);
				if(!file) return "Error: cannot create the temporary API request";
				file << requestBody.dump();
			}

			const std::string command =
				"curl -sS --connect-timeout 10 --max-time 90"
				" -X POST https://api.anthropic.com/v1/messages"
				" -H " + shellQuote("content-type: application/json") +
				" -H " + shellQuote("x-api-key: " + apiKey) +
				" -H " + shellQuote("anthropic-version: 2023-06-01") +
				" -d @" + shellQuote(tmpPath) + " 2>&1";

			FILE* pipe = popen(command.c_str(), "r");
			if(!pipe) {
				std::remove(tmpPath.c_str());
				return "Error: could not start curl";
			}

			std::string response;
			char chunk[4096];
			while(fgets(chunk, sizeof(chunk), pipe)) response += chunk;
			const int curlStatus = pclose(pipe);
			std::remove(tmpPath.c_str());

			if(response.empty()) {
				return curlStatus == 0 ? "Error: empty API response" : "Error: API request failed";
			}

			ofJson object = ofJson::parse(response);
			if(object.contains("error")) {
				return "API error: " + object["error"].value("message", "unknown error");
			}
			if(!object.contains("content") || !object["content"].is_array() || object["content"].empty()) {
				return "Error: unexpected API response";
			}
			return object["content"][0].value("text", "");
		} catch(const std::exception& e) {
			return std::string("Error: ") + e.what();
		}
	}

	void requestLLMFormula(const std::string& userPrompt) {
		if(llmPending.load()) return;
		if(llmApiKey.empty()) {
			llmStatusMsg = "No API key";
			llmHasError = true;
			llmDone.store(true);
			return;
		}

		llmPending.store(true);
		llmDone.store(false);
		llmHasError = false;
		llmStatusMsg = "Generating...";

		const std::string currentFormula = formulaString.get();
		const std::string systemPrompt = buildFormulaSystemPrompt();
		const std::string apiKey = llmApiKey;
		std::string fullPrompt;

		if(llmFixMode) {
			const std::string issue = userPrompt.empty()
				? "Fix any syntax or logic problems while preserving the apparent intent."
				: "Requested change or issue: " + userPrompt;
			fullPrompt = "Fix this Formula-node expression. Preserve its current port numbers and labels unless the request requires changing them.\n\nCurrent formula:\n" +
				currentFormula + "\n\n" + issue;
		} else {
			fullPrompt = "Create a Formula-node expression. Choose as many $iN inputs and $oN outputs as the request needs; the node creates them automatically. Add concise .label metadata when useful.\n\nUser request:\n" + userPrompt;
		}

		llmFuture = std::async(std::launch::async,
			[fullPrompt, systemPrompt, apiKey]() {
				return callAnthropicAPI(fullPrompt, systemPrompt, apiKey);
			});
	}

	void pollLLMResult() {
		if(!llmPending.load() || !llmFuture.valid()) return;
		if(llmFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

		std::string result = llmFuture.get();
		const bool apiError = result.rfind("Error:", 0) == 0 ||
			result.rfind("API error:", 0) == 0;
		if(apiError) {
			llmStatusMsg = result;
			llmHasError = true;
		} else {
			result = stripCodeFences(result);
			formulaBuf = result;
			formulaString.set(result);
			previousFormula = result;
			rebuildEvaluator();
			calculate();

			llmHasError = !formulaValid;
			llmStatusMsg = formulaValid
				? "AI formula ready"
				: "AI formula needs editing: " + lastError;
		}

		llmPending.store(false);
		llmDone.store(true);
	}

	void drawAIPopup() {
		ImGui::SetNextWindowSize(ImVec2(520, 235), ImGuiCond_Appearing);
		if(!ImGui::BeginPopup("##formula_ai_popup", ImGuiWindowFlags_None)) return;

		ImGui::TextColored(ImVec4(0.72f, 0.60f, 1.0f, 1.0f),
			"Ask Claude to create a Formula expression");
		ImGui::SameLine();
		if(llmApiKey.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "  [No API key]");
		} else {
			ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "  [Key loaded]");
		}
		ImGui::TextDisabled("Ports are created automatically from $iN, legacy $N, and $oN references.");
		ImGui::Spacing();

		auto modeButton = [](const char* label, bool active) {
			ImGui::PushStyleColor(ImGuiCol_Button,
				active ? IM_COL32(65, 40, 95, 255) : IM_COL32(28, 28, 38, 255));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 58, 130, 255));
			const bool clicked = ImGui::Button(label, ImVec2(122, 0));
			ImGui::PopStyleColor(2);
			return clicked;
		};
		if(modeButton("Generate##formula", !llmFixMode)) llmFixMode = false;
		ImGui::SameLine(0, 2);
		if(modeButton("Fix current##formula", llmFixMode)) llmFixMode = true;

		ImGui::Separator();
		ImGui::Spacing();

		if(llmPending.load()) {
			static const char* spinner = "|/-\\";
			static int spinnerIndex = 0;
			spinnerIndex = (spinnerIndex + 1) % 4;
			ImGui::Text("Generating... %c", spinner[spinnerIndex]);
			ImGui::TextDisabled("The result will replace the current formula when ready.");
		} else if(llmDone.load() && llmHasError) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
			ImGui::TextWrapped("%s", llmStatusMsg.c_str());
			ImGui::PopStyleColor();
			ImGui::Spacing();
			if(ImGui::Button("Back##formula_ai", ImVec2(80, 0))) {
				llmDone.store(false);
				llmHasError = false;
			}
			ImGui::SameLine();
			if(ImGui::Button("Close##formula_ai", ImVec2(80, 0))) {
				llmDone.store(false);
				ImGui::CloseCurrentPopup();
			}
		} else if(llmDone.load()) {
			llmDone.store(false);
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::InputTextMultiline("##formula_ai_prompt", llmPromptBuf,
				sizeof(llmPromptBuf), ImVec2(504, 100), ImGuiInputTextFlags_WordWrap);
			ImGui::TextDisabled(llmFixMode
				? "Describe the issue, or leave blank for a general repair."
				: "Describe the vector or mathematical transformation you want.");
			ImGui::Spacing();

			const bool canSubmit = !llmApiKey.empty() &&
				(llmFixMode || llmPromptBuf[0] != '\0');
			if(!canSubmit) ImGui::BeginDisabled();
			if(ImGui::Button(llmFixMode ? "Fix formula" : "Generate", ImVec2(100, 0))) {
				requestLLMFormula(llmPromptBuf);
			}
			if(!canSubmit) ImGui::EndDisabled();
			ImGui::SameLine(0, 10);
			if(ImGui::Button("Cancel##formula_ai", ImVec2(70, 0))) {
				ImGui::CloseCurrentPopup();
			}

			if(llmApiKey.empty()) {
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(1, 0.65f, 0.3f, 1), "No API key found. Add one of:");
				ImGui::TextDisabled("  data/anthropic_api_key.txt");
				ImGui::TextDisabled("  ~/.anthropic_api_key");
				ImGui::TextDisabled("  ANTHROPIC_API_KEY environment variable");
			}
		}

		ImGui::EndPopup();
	}

	// ===== Formula library =====
	static std::string sanitizeFormulaName(const std::string& value) {
		std::string name = trimStr(value);
		if(ofToLower(ofFilePath::getFileExt(name)) == "json") {
			name = ofFilePath::getBaseName(name);
		}

		std::string sanitized;
		sanitized.reserve(name.size());
		for(char c : name) {
			if(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
				sanitized.push_back(c);
			} else if(std::isspace(static_cast<unsigned char>(c))) {
				sanitized.push_back('_');
			}
		}
		return sanitized.empty() ? "formula" : sanitized;
	}

	static std::string getFormulaDirectory() {
		return ofToDataPath("formulas", true);
	}

	static void ensureFormulaDirectoryExists() {
		const std::string directory = getFormulaDirectory();
		if(!ofDirectory::doesDirectoryExist(directory, false)) {
			ofDirectory::createDirectory(directory, false, true);
		}
	}

	void saveFormulaFile() {
		ensureFormulaDirectoryExists();

		const std::string name = sanitizeFormulaName(formulaName.get());
		const std::string path = ofFilePath::join(getFormulaDirectory(), name + ".json");
		ofJson json = {
			{"name", name},
			{"description", "User formula"},
			{"formula", formulaString.get()}
		};

		if(ofSavePrettyJson(path, json)) {
			formulaName.setWithoutEventNotifications(name);
			ofLogNotice("Formula") << "Saved formula: " << path;
		} else {
			ofLogError("Formula") << "Could not save formula: " << path;
		}
	}

	void loadFormulaFile() {
		ensureFormulaDirectoryExists();
		ofFileDialogResult result = ofSystemLoadDialog(
			"Load Formula",
			false,
			getFormulaDirectory());
		if(!result.bSuccess) return;

		const std::string path = result.getPath();
		if(ofDirectory::doesDirectoryExist(path, false)) {
			ofLogWarning("Formula") << "Please select a formula file, not a folder";
			return;
		}

		try {
			std::string loadedFormula;
			std::string loadedName = ofFilePath::getBaseName(path);

			if(ofToLower(ofFilePath::getFileExt(path)) == "json") {
				ofJson json = ofLoadJson(path);
				if(!json.contains("formula") || !json["formula"].is_string()) {
					throw std::runtime_error("JSON file has no string 'formula' field");
				}
				loadedFormula = json["formula"].get<std::string>();
				if(json.contains("name") && json["name"].is_string()) {
					loadedName = json["name"].get<std::string>();
				}
			} else {
				ofBuffer buffer = ofBufferFromFile(path);
				if(buffer.size() == 0) throw std::runtime_error("File is empty or unreadable");
				loadedFormula = buffer.getText();
			}

			formulaName.setWithoutEventNotifications(sanitizeFormulaName(loadedName));
			formulaString.set(loadedFormula);
			formulaBuf = loadedFormula;
			previousFormula = loadedFormula;
			rebuildEvaluator();
			calculate();
			ofLogNotice("Formula") << "Loaded formula: " << path;
		} catch(const std::exception& e) {
			ofLogError("Formula") << "Could not load formula '" << path << "': " << e.what();
		}
	}

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

	enum class PortKind { None, LegacyInput, NamedInput, Output };

	struct PortLayout {
		std::set<int> inputs;
		std::set<int> namedInputs;
		std::set<int> outputs;
		std::map<int, std::string> inputLabels;
		std::map<int, std::string> outputLabels;
	};

	// A compiled statement: either an assignment (target non-empty) or a bare
	// expression. Bare expressions are allowed only in legacy single-output mode.
	struct Statement {
		std::string target;   // empty => legacy Output expression
		RPN code;
	};

	std::vector<Statement> program;
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

	// ===== Dynamic ports =====
	void onFormulaParamChanged(std::string &s){
		if(s != formulaBuf) formulaBuf = s;
	}

	static PortKind parsePortIdentifier(const std::string& id, int& number) {
		number = 0;
		if(id.size() < 2 || id[0] != '$') return PortKind::None;

		PortKind kind = PortKind::None;
		size_t digitStart = 1;
		if(std::isdigit(static_cast<unsigned char>(id[1]))) {
			kind = PortKind::LegacyInput;
		} else if(id[1] == 'i') {
			kind = PortKind::NamedInput;
			digitStart = 2;
		} else if(id[1] == 'o') {
			kind = PortKind::Output;
			digitStart = 2;
		} else {
			return PortKind::None;
		}

		if(digitStart >= id.size() || id[digitStart] == '0') return PortKind::None;
		for(size_t i = digitStart; i < id.size(); ++i) {
			if(!std::isdigit(static_cast<unsigned char>(id[i]))) return PortKind::None;
		}

		try {
			const long long parsed = std::stoll(id.substr(digitStart));
			if(parsed <= 0 || parsed > INT_MAX) return PortKind::None;
			number = static_cast<int>(parsed);
			return kind;
		} catch(...) {
			return PortKind::None;
		}
	}

	static bool parseLabelDeclaration(const std::string& segment,
									  PortKind& kind,
									  int& number,
									  std::string& label) {
		const std::string seg = trimStr(segment);
		const size_t labelPos = seg.find(".label");
		if(labelPos == std::string::npos) return false;

		const std::string portName = trimStr(seg.substr(0, labelPos));
		kind = parsePortIdentifier(portName, number);
		if(kind == PortKind::None) return false;

		size_t pos = labelPos + 6;
		while(pos < seg.size() && std::isspace(static_cast<unsigned char>(seg[pos]))) ++pos;
		if(pos >= seg.size() || seg[pos] != '=') {
			throw std::runtime_error("Expected '=' after '" + portName + ".label'");
		}

		label = trimStr(seg.substr(pos + 1));
		if(label.size() >= 2 &&
		   ((label.front() == '"' && label.back() == '"') ||
			(label.front() == '\'' && label.back() == '\''))) {
			label = label.substr(1, label.size() - 2);
		}
		label = trimStr(label);
		if(label.empty()) throw std::runtime_error("Port label cannot be empty");
		return true;
	}

	static std::string chooseUniquePortName(const std::string& preferred,
										const std::string& canonical,
										std::set<std::string>& used) {
		auto available = [&used](const std::string& candidate) {
			return !candidate.empty() && used.count(candidate) == 0 && candidate != "Formula Editor";
		};

		std::string chosen = trimStr(preferred);
		if(!available(chosen)) chosen = canonical;
		if(!available(chosen)) {
			int suffix = 2;
			do {
				chosen = canonical + " " + ofToString(suffix++);
			} while(!available(chosen));
		}
		used.insert(chosen);
		return chosen;
	}

	void addInputPort(int number, const std::string& name) {

		auto paramRef = std::make_shared<ofParameter<std::vector<float>>>();
		paramRef->set(name, {0}, {-FLT_MAX}, {FLT_MAX});
		inputParamRefs[number] = paramRef;

		auto oceaParam = addParameter(*paramRef);
		inputParameters[number] = oceaParam;

		inputListeners[number] = paramRef->newListener([this](std::vector<float>&){ calculate(); });
	}

	void addOutputPort(int number, const std::string& name) {
		auto paramRef = std::make_shared<ofParameter<std::vector<float>>>();
		paramRef->set(name, {0}, {-FLT_MAX}, {FLT_MAX});
		outputParamRefs[number] = paramRef;
		outputParameters[number] = addOutputParameter(*paramRef);
	}

	void syncPorts(const PortLayout& layout) {
		std::set<int> desiredOutputs = layout.outputs;
		if(desiredOutputs.empty()) desiredOutputs.insert(0); // legacy Output

		// Remove ports which no longer occur in the successfully compiled formula.
		for(auto it = inputParameters.begin(); it != inputParameters.end();) {
			if(layout.inputs.count(it->first) == 0) {
				const int number = it->first;
				removeParameter(it->second->getEscapedName());
				inputListeners.erase(number);
				inputParamRefs.erase(number);
				it = inputParameters.erase(it);
			} else {
				++it;
			}
		}
		for(auto it = outputParameters.begin(); it != outputParameters.end();) {
			if(desiredOutputs.count(it->first) == 0) {
				const int number = it->first;
				removeParameter(it->second->getEscapedName());
				outputParamRefs.erase(number);
				it = outputParameters.erase(it);
			} else {
				++it;
			}
		}

		std::set<std::string> usedNames;
		std::map<int, std::string> inputNames;
		std::map<int, std::string> outputNames;
		for(int number : layout.inputs) {
			const std::string canonical = (layout.namedInputs.count(number) > 0 ? "$i" : "$") +
				ofToString(number);
			auto labelIt = layout.inputLabels.find(number);
			const std::string preferred = labelIt == layout.inputLabels.end() ? canonical : labelIt->second;
			inputNames[number] = chooseUniquePortName(preferred, canonical, usedNames);
		}
		for(int number : desiredOutputs) {
			const std::string canonical = number == 0 ? "Output" : "$o" + ofToString(number);
			auto labelIt = layout.outputLabels.find(number);
			const std::string preferred = labelIt == layout.outputLabels.end() ? canonical : labelIt->second;
			outputNames[number] = chooseUniquePortName(preferred, canonical, usedNames);
		}

		// Move retained ports through unique temporary names so labels can be
		// swapped without colliding inside the parameter group.
		for(auto& entry : inputParameters) {
			if(entry.second->getName() != inputNames[entry.first]) {
				entry.second->setName("__formula_input_" + ofToString(entry.first) + "__");
			}
		}
		for(auto& entry : outputParameters) {
			if(entry.second->getName() != outputNames[entry.first]) {
				entry.second->setName("__formula_output_" + ofToString(entry.first) + "__");
			}
		}

		for(int number : layout.inputs) {
			if(inputParameters.count(number) == 0) addInputPort(number, inputNames[number]);
			else if(inputParameters[number]->getName() != inputNames[number])
				inputParameters[number]->setName(inputNames[number]);
		}
		for(int number : desiredOutputs) {
			if(outputParameters.count(number) == 0) addOutputPort(number, outputNames[number]);
			else if(outputParameters[number]->getName() != outputNames[number])
				outputParameters[number]->setName(outputNames[number]);
		}
	}

	// ===== Public evaluation =====
	void calculate() {
		auto setAllOutputsToZero = [this]() {
			for(auto& entry : outputParamRefs) entry.second->set(std::vector<float>{0.0f});
		};
		if(!formulaValid) {
			setAllOutputsToZero();
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
			int number = kv.first;
			const auto& vec = kv.second->get();
			Value value = vec.size() <= 1
				? (vec.empty() ? Value::scalar(0.0f) : Value::scalar(vec[0]))
				: Value::vector(vec);
			env["$" + ofToString(number)] = value;
			env["$i" + ofToString(number)] = value;
		}
		for(const auto& kv : outputParamRefs) {
			if(kv.first > 0) env["$o" + ofToString(kv.first)] = Value::scalar(0.0f);
		}
		env["pi"] = Value::scalar(float(M_PI)); env["PI"] = env["pi"];
		env["e"]  = Value::scalar(float(M_E));  env["E"]  = env["e"];
		env["N"]  = Value::scalar(float(N));    // optional helper

		try{
			Value legacyResult;
			bool haveLegacyResult = false;

			for(const auto& stmt : program){
				Value r = evalRPN(stmt.code, env);
				if(stmt.target.empty()){ legacyResult = r; haveLegacyResult = true; }
				else                    { env[stmt.target] = r; }
			}

			auto toVector = [](const Value& value) {
				return value.isVec
					? (value.v.empty() ? std::vector<float>{0.0f} : value.v)
					: std::vector<float>{value.f};
			};

			for(auto& entry : outputParamRefs) {
				if(entry.first == 0) {
					entry.second->set(haveLegacyResult ? toVector(legacyResult) : std::vector<float>{0.0f});
				} else {
					auto valueIt = env.find("$o" + ofToString(entry.first));
					entry.second->set(valueIt == env.end() ? std::vector<float>{0.0f} : toVector(valueIt->second));
				}
			}
		}
		catch(const std::exception& e){
			ofLogError("Formula") << "Eval error: " << e.what();
			setAllOutputsToZero();
		}
	}


	// ===== Compiler pipeline =====
	void rebuildEvaluator() {
		lastError.clear();
		formulaValid = false;

		std::string src = formulaString.get();
		if(trimStr(src).empty()) {
			lastError = "Empty formula";
			ofLogError("Formula") << lastError;
			return;
		}

		try {
			// Split the source into ';'-separated statements. The language has no
			// string or comment literals, so a plain split is unambiguous.
			std::vector<std::string> segments;
			{
				std::string cur;
				for(char c : src){
					if(c == ';'){ segments.push_back(cur); cur.clear(); }
					else          cur.push_back(c);
				}
				segments.push_back(cur);
			}

			std::vector<Statement> prog;
			PortLayout layout;
			std::map<std::string, std::string> portAliases;
			bool hasOutputAssignment = false;

			// Collect labels before compiling expressions so aliases may be used
			// before or after their metadata declaration.
			for(const auto& rawSeg : segments) {
				const std::string seg = trimStr(rawSeg);
				if(seg.empty()) continue;

				PortKind labelKind = PortKind::None;
				int labelNumber = 0;
				std::string label;
				if(!parseLabelDeclaration(seg, labelKind, labelNumber, label)) continue;

				if(!isPlainIdentifier(label)) {
					throw std::runtime_error("Port label '" + label +
						"' is invalid (use letters, digits, and underscores; no spaces)");
				}
				if(isReservedName(label) || isFunction(label)) {
					throw std::runtime_error("Port label '" + label + "' is reserved");
				}
				if(portAliases.count(label) > 0) {
					throw std::runtime_error("Duplicate port label '" + label + "'");
				}

				std::string canonical;
				if(labelKind == PortKind::Output) {
					if(layout.outputLabels.count(labelNumber) > 0)
						throw std::runtime_error("Output $o" + ofToString(labelNumber) + " has more than one label");
					layout.outputs.insert(labelNumber);
					layout.outputLabels[labelNumber] = label;
					canonical = "$o" + ofToString(labelNumber);
				} else {
					if(layout.inputLabels.count(labelNumber) > 0)
						throw std::runtime_error(std::string("Input ") +
							(labelKind == PortKind::NamedInput ? "$i" : "$") + ofToString(labelNumber) +
							" has more than one label");
					layout.inputs.insert(labelNumber);
					if(labelKind == PortKind::NamedInput) layout.namedInputs.insert(labelNumber);
					layout.inputLabels[labelNumber] = label;
					canonical = (labelKind == PortKind::NamedInput ? "$i" : "$") + ofToString(labelNumber);
				}
				portAliases[label] = canonical;
			}

			auto recordIdentifier = [&layout](const std::string& identifier) {
				int number = 0;
				const PortKind kind = parsePortIdentifier(identifier, number);
				if(kind == PortKind::None) {
					if(!identifier.empty() && identifier[0] == '$') {
						throw std::runtime_error("Invalid port identifier '" + identifier +
							"' (use $i1, $o1, or legacy $1)");
					}
					return;
				}
				if(kind == PortKind::Output) {
					layout.outputs.insert(number);
				} else {
					layout.inputs.insert(number);
					if(kind == PortKind::NamedInput) layout.namedInputs.insert(number);
				}
			};

			for(const auto& rawSeg : segments){
				std::string seg = trimStr(rawSeg);
				if(seg.empty()) continue;   // tolerates a trailing ';' and blank statements

				PortKind labelKind = PortKind::None;
				int labelNumber = 0;
				std::string label;
				if(parseLabelDeclaration(seg, labelKind, labelNumber, label)) continue;

				std::string name, expr;
				Statement st;
				if(splitAssignment(seg, name, expr)){
					auto aliasIt = portAliases.find(name);
					if(aliasIt != portAliases.end()) name = aliasIt->second;
					if(isFunction(name))
						throw std::runtime_error("'" + name + "' is a built-in function and cannot be used as a variable");
					if(isReservedName(name))
						throw std::runtime_error("'" + name + "' is a reserved name and cannot be used as a variable");
					if(!name.empty() && name[0] == '$') {
						int targetNumber = 0;
						const PortKind targetKind = parsePortIdentifier(name, targetNumber);
						if(targetKind == PortKind::LegacyInput || targetKind == PortKind::NamedInput)
							throw std::runtime_error("Cannot assign to input '" + name + "'");
						if(targetKind != PortKind::Output)
							throw std::runtime_error("Invalid assignment target '" + name + "'");
					}
					if(trimStr(expr).empty())
						throw std::runtime_error("Assignment to '" + name + "' has no expression");
					st.target = name;
					int targetNumber = 0;
					if(parsePortIdentifier(name, targetNumber) == PortKind::Output) {
						hasOutputAssignment = true;
						recordIdentifier(name);
					}
					auto tokens = tokenize(expr);
					for(auto& token : tokens) {
						if(token.type != Token::Identifier) continue;
						auto tokenAliasIt = portAliases.find(token.text);
						if(tokenAliasIt != portAliases.end()) token.text = tokenAliasIt->second;
					}
					for(const auto& token : tokens) {
						if(token.type == Token::Identifier) recordIdentifier(token.text);
					}
					st.code = shuntingYard(tokens);
				}else{
					st.target.clear();
					auto tokens = tokenize(seg);
					for(auto& token : tokens) {
						if(token.type != Token::Identifier) continue;
						auto tokenAliasIt = portAliases.find(token.text);
						if(tokenAliasIt != portAliases.end()) token.text = tokenAliasIt->second;
					}
					for(const auto& token : tokens) {
						if(token.type == Token::Identifier) recordIdentifier(token.text);
					}
					st.code = shuntingYard(tokens);
				}
				prog.push_back(std::move(st));
			}

			if(prog.empty()) throw std::runtime_error("Empty formula");

			if(layout.outputs.empty()) {
				for(size_t k = 0; k + 1 < prog.size(); ++k){
					if(prog[k].target.empty())
						throw std::runtime_error("Statement " + ofToString((int)k + 1) +
							" has no effect: only the last statement may be a bare expression");
				}
				if(!prog.back().target.empty())
					throw std::runtime_error("Formula must end with an expression, but the last statement assigns to '" +
						prog.back().target + "'");
			} else {
				for(size_t k = 0; k < prog.size(); ++k) {
					if(prog[k].target.empty()) {
						throw std::runtime_error("Statement " + ofToString((int)k + 1) +
							" is a bare expression; assign explicit formulas to $o1, $o2, etc.");
					}
				}
				if(!hasOutputAssignment)
					throw std::runtime_error("At least one explicit output must be assigned, for example $o1 = $i1");
			}

			syncPorts(layout);
			program = std::move(prog);
			formulaValid = true;
		} catch(const std::exception& e) {
			lastError = e.what();
			ofLogError("Formula") << "Parse error: " << lastError;
		}
	}

	// ===== Statement helpers =====
	static std::string trimStr(const std::string& s){
		size_t a = 0, b = s.size();
		while(a < b && std::isspace((unsigned char)s[a]))   a++;
		while(b > a && std::isspace((unsigned char)s[b-1])) b--;
		return s.substr(a, b - a);
	}

	static bool isReservedName(const std::string& n){
		static const std::set<std::string> reserved = { "pi", "PI", "e", "E", "N", "__veclit" };
		return reserved.count(n) > 0;
	}

	static bool isPlainIdentifier(const std::string& name) {
		if(name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_'))
			return false;
		for(size_t i = 1; i < name.size(); ++i) {
			if(!(std::isalnum(static_cast<unsigned char>(name[i])) || name[i] == '_')) return false;
		}
		return true;
	}

	// Recognises "name = expr". Returns false for anything else, including
	// comparisons such as "a == b", which stay ordinary expressions.
	static bool splitAssignment(const std::string& seg, std::string& name, std::string& expr){
		size_t i = 0;
		while(i < seg.size() && std::isspace((unsigned char)seg[i])) i++;
		if(i >= seg.size() || !isIdentStart(seg[i])) return false;

		size_t start = i++;
		while(i < seg.size() && isIdentChar(seg[i])) i++;
		std::string id = seg.substr(start, i - start);

		size_t j = i;
		while(j < seg.size() && std::isspace((unsigned char)seg[j])) j++;
		if(j >= seg.size() || seg[j] != '=')        return false;   // not an assignment
		if(j + 1 < seg.size() && seg[j + 1] == '=') return false;   // "==" comparison

		if(id[0] == '$') {
			int number = 0;
			const PortKind kind = parsePortIdentifier(id, number);
			if(kind == PortKind::LegacyInput || kind == PortKind::NamedInput)
				throw std::runtime_error("Cannot assign to input '" + id + "'");
			if(kind != PortKind::Output)
				throw std::runtime_error("Invalid assignment target '" + id +
					"' (outputs use $o1, $o2, etc.)");
		}

		name = id;
		expr = seg.substr(j + 1);
		return true;
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
			"vec","repeat","concat","sort","pairdist"
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

				// small helpers for vector extraction
				auto toVec = [&](const Value& v)->std::vector<float> {
					return v.isVec ? v.v : std::vector<float>{ v.f };
				};

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
				if(is("sort")){
					if(argc!=1) throw std::runtime_error("sort(v) expects 1 arg");
					auto out = toVec(pop1());
					std::sort(out.begin(), out.end());
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

				// -------- NEW: pairwise distances --------
				if (is("pairdist")) {
					if (argc != 2) throw std::runtime_error("pairdist(x, y) expects 2 args");
					Value vy = pop1();
					Value vx = pop1();
					std::vector<float> x = toVec(vx);
					std::vector<float> y = toVec(vy);

					size_t n = std::min(x.size(), y.size());
					if (n < 2) { st.push_back(Value::vector({})); continue; }

					std::vector<float> out;
					out.reserve(n * (n - 1) / 2);
					for (size_t i = 0; i + 1 < n; ++i) {
						float xi = x[i], yi = y[i];
						for (size_t j = i + 1; j < n; ++j) {
							float dx = xi - x[j];
							float dy = yi - y[j];
							out.push_back(std::sqrt(dx*dx + dy*dy));
						}
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
