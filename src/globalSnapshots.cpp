#include "globalSnapshots.h"
#include "imgui.h"
#include "ofxOceanodeNodeMacro.h"
#include <vector>
#include <cmath>
#include <unordered_set>
#include "ofMain.h" // for ofGetElapsedTimef / ofGetElapsedTimeMillis

// --- small helpers ---
static void customPow(float & value, float pow){
	float k1 = 2*pow*0.99999;
	float k2 = (k1/((-pow*0.999999)+1));
	float k3 = k2 * std::fabs(value) + 1;
	value = value * (k2+1) / k3;
}

static bool floatsEqual(float a, float b, float eps = 1e-6f) {
	return std::fabs(a - b) <= eps;
}

static bool floatVectorsEqual(const std::vector<float>& a,
							  const std::vector<float>& b,
							  float eps = 1e-6f) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i) {
		if (std::fabs(a[i] - b[i]) > eps) return false;
	}
	return true;
}

globalSnapshots::globalSnapshots()
: ofxOceanodeNodeModel("Global Snapshots")
, currentSnapshotSlot(-1)
, globalContainer(nullptr)
, isInterpolating(false)
, interpolationStartTime(0)
, interpolationTargetSlot(-1)
, interpolationBiPowValue(0.0f)
{
}

void globalSnapshots::setContainer(ofxOceanodeContainer* c) {
	globalContainer = c;
	ofxOceanodeNodeModel::setContainer(c);
}

void globalSnapshots::setup() {
	setup("");
}

void globalSnapshots::setup(std::string additionalInfo) {
	description = "Global snapshot system that captures all parameters in the current canvas. Shift+click to store, click to recall.";
	
	matrixRows.set("Rows", 2, 1, 8);
	matrixCols.set("Cols", 8, 1, 8);
	buttonSize.set("Button Size", 28.0f, 15.0f, 600.0f);
	showSnapshotNames.set("Show Names", true);
	includeMacroParams.set("Include Macro Params", false);
	interpolationMs.set("Interpolation Ms", 0.0f, 0.0f, 60000.0f);
	biPow.set("BiPow", 0.0f, -1.0f, 1.0f);
	transition.set("Transition", 0.0f, 0.0f, 1.0f);
	done.set("Done");
	
	addInspectorParameter(includeMacroParams);
	addInspectorParameter(matrixRows);
	addInspectorParameter(matrixCols);
	addInspectorParameter(buttonSize);
	addInspectorParameter(showSnapshotNames);

	addInspectorParameter(addSnapshotButton.set("Add Snapshot"));
	addSnapshotListener = addSnapshotButton.newListener([this](){
		int newSlot = snapshots.empty() ? 0 : (snapshots.rbegin()->first + 1);
		storeSnapshot(newSlot);
	});

	addParameter(interpolationMs);
	addParameter(biPow);
	addParameter(activeSnapshotSlot.set(
		"Slot",
		-1,
		-1,
		matrixRows.get() * matrixCols.get() - 1
	));
	addOutputParameter(transition);
	addOutputParameter(done);
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int &slot){
		if(slot >= 0) {
			if(interpolationMs.get() > 0) {
				startInterpolation(slot);
			} else {
				loadSnapshot(slot);
			}
		}
	});

	addCustomRegion(snapshotControlGui.set("Snapshots", [this](){
		renderSnapshotMatrix();
	}), [this](){
		renderSnapshotMatrix();
	});

	addInspectorParameter(snapshotInspector.set("Snapshot Names", [this](){
		renderInspectorInterface();
	}));
	
	loadSnapshotsFromFile();
}

void globalSnapshots::update(ofEventArgs &e) {
	if(isInterpolating) {
		updateInterpolation();
	}
}

// ----------------------------------------------------------
// Automatic exclusion: parameters with incoming connections
// ----------------------------------------------------------
bool globalSnapshots::isParameterModulated(const std::string& key) const {
	if(!globalContainer) return false;

	const auto &conns = globalContainer->getAllConnections();
	for(const auto &c : conns) {
		if(!c) continue;

		const auto &sinkParam = c->getSinkParameter();
		std::string destKey = sinkParam.getGroupHierarchyNames()[0] + "/" + sinkParam.getName();

		if(destKey == key) {
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------
// Combined exclusion:
// 1) manual blacklist
// 2) modulated
// 3) flagged as output → DisableInConnection
// ----------------------------------------------------------
bool globalSnapshots::isParameterExcluded(const std::string& key, ofxOceanodeAbstractParameter* param) const {
	if(manualExcludes.find(key) != manualExcludes.end()) return true;
	if(isParameterModulated(key)) return true;
	if(param) {
		auto flags = param->getFlags();
		if(flags & ofxOceanodeParameterFlags_DisableInConnection) {
			return true;
		}
	}
	return false;
}

bool globalSnapshots::shouldSkipInterpolation(const std::string& key) const {
	// Parameters that should be stored but NOT interpolated
	// They will be applied instantly when recalling a snapshot
	return (key.find("/Interpolation Ms") != std::string::npos ||
	        key.find("/BiPow") != std::string::npos);
}

void globalSnapshots::storeSnapshot(int slot) {
	if (!globalContainer) {
		ofLogError("globalSnapshots") << "No global container set";
		return;
	}
	
	SnapshotData data;
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		data.name = it->second.name;
	} else {
		data.name = ofToString(slot);
	}

	int paramCount = 0;

	for(auto *node : globalContainer->getAllModules()) {
		if (!node) continue;
		
		if(!includeMacroParams.get() && dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
			continue;
		}
		
		auto &grp = node->getParameters();
		std::string grpName = grp.getEscapedName();
		
		// Check if this is a globalSnapshots module (we'll store BiPow for all globalSnapshots instances)
		bool isOwnNode = (grpName.find("Global_Snapshots") == 0);

		for(int i = 0; i < grp.size(); i++) {
			auto &p = grp.get(i);
			auto *oParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&p);
			if(!oParam) continue;

			std::string key = grpName + "/" + p.getName();
			
			// Allow this node's own input parameters to be stored, even if normally excluded
			if(!isOwnNode && isParameterExcluded(key, oParam)) {
				continue;
			}

			ParameterSnapshot ps;
			ps.type = oParam->valueType();

			try {
				if(ps.type == typeid(float).name()) {
					ps.value = oParam->cast<float>().getParameter().get();
					paramCount++;
				}
				else if(ps.type == typeid(int).name()) {
					ps.value = static_cast<float>(oParam->cast<int>().getParameter().get());
					paramCount++;
				}
				else if(ps.type == typeid(bool).name()) {
					ps.value = oParam->cast<bool>().getParameter().get();
					paramCount++;
				}
				else if(ps.type == typeid(std::string).name()) {
					ps.value = oParam->cast<std::string>().getParameter().get();
					paramCount++;
				}
				else if(ps.type == typeid(std::vector<float>).name()) {
					ps.value = oParam->cast<std::vector<float>>().getParameter().get();
					paramCount++;
				}
				else if(ps.type == typeid(std::vector<int>).name()) {
					auto vi = oParam->cast<std::vector<int>>().getParameter().get();
					std::vector<float> vf(vi.begin(), vi.end());
					ps.value = vf;
					paramCount++;
				}
				
				data.paramValues[key] = std::move(ps);
			} catch(...) {
				ofLogWarning("globalSnapshots") << "Failed to capture parameter: " << key;
			}
		}
	}

	snapshots[slot] = std::move(data);
	currentSnapshotSlot = slot;
	
	saveSnapshotsToFile();
	
	ofLogNotice("globalSnapshots") << "Stored snapshot " << slot << " with " << paramCount << " parameters";
}

void globalSnapshots::loadSnapshot(int slot) {
	if (!globalContainer) {
		ofLogError("globalSnapshots") << "No global container set";
		return;
	}
	
	auto it = snapshots.find(slot);
	if(it == snapshots.end()) return;

	int loadedCount = 0;
	for(auto *node : globalContainer->getAllModules()) {
		if (!node) continue;
		
		if(!includeMacroParams.get() && dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
			continue;
		}
		
		auto &grp = node->getParameters();
		std::string grpName = grp.getEscapedName();

		for(int i = 0; i < grp.size(); i++) {
			auto &p = grp.get(i);
			auto *oParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&p);
			if(!oParam) continue;

			std::string key = grpName + "/" + p.getName();
			auto pit = it->second.paramValues.find(key);
			if(pit == it->second.paramValues.end()) continue;

			if(isParameterExcluded(key, oParam)) {
				continue;
			}

			auto &ps = pit->second;
			try {
				if(ps.type == typeid(float).name()) {
					float currentVal = oParam->cast<float>().getParameter().get();
					float targetVal  = ps.value.get<float>();
					if(!floatsEqual(currentVal, targetVal)) {
						oParam->cast<float>().getParameter() = targetVal;
						loadedCount++;
					}
				}
				else if(ps.type == typeid(int).name()) {
					int currentVal = oParam->cast<int>().getParameter().get();
					int targetVal  = (int)std::round(ps.value.get<float>());
					if(currentVal != targetVal) {
						oParam->cast<int>().getParameter() = targetVal;
						loadedCount++;
					}
				}
				else if(ps.type == typeid(bool).name()) {
					bool currentVal = oParam->cast<bool>().getParameter().get();
					bool targetVal  = ps.value.get<bool>();
					if(currentVal != targetVal) {
						oParam->cast<bool>().getParameter() = targetVal;
						loadedCount++;
					}
				}
				else if(ps.type == typeid(std::string).name()) {
					std::string currentVal = oParam->cast<std::string>().getParameter().get();
					std::string targetVal  = ps.value.get<std::string>();
					if(currentVal != targetVal) {
						oParam->cast<std::string>().getParameter() = targetVal;
						loadedCount++;
					}
				}
				else if(ps.type == typeid(std::vector<float>).name()) {
					auto currentVal = oParam->cast<std::vector<float>>().getParameter().get();
					auto targetVal  = ps.value.get<std::vector<float>>();
					if(!floatVectorsEqual(currentVal, targetVal)) {
						oParam->cast<std::vector<float>>().getParameter() = targetVal;
						loadedCount++;
					}
				}
				else if(ps.type == typeid(std::vector<int>).name()) {
					auto currentVal = oParam->cast<std::vector<int>>().getParameter().get();
					auto targetF    = ps.value.get<std::vector<float>>();
					std::vector<int> targetVal;
					targetVal.reserve(targetF.size());
					for(auto f : targetF) targetVal.push_back((int)std::round(f));
					if(currentVal != targetVal) {
						oParam->cast<std::vector<int>>().getParameter() = targetVal;
						loadedCount++;
					}
				}
			} catch(...) {
				ofLogWarning("globalSnapshots") << "Failed to load parameter: " << key;
			}
		}
	}

	currentSnapshotSlot = slot;
	ofLogNotice("globalSnapshots") << "Loaded snapshot " << slot << " - " << loadedCount << " parameters changed";
}

void globalSnapshots::renderSnapshotMatrix() {
	ImGui::PushID("GlobalSnapshots");
	int rows = matrixRows.get();
	int cols = matrixCols.get();

	// for blinking
	float t = ofGetElapsedTimef();
	float blink = 0.5f + 0.5f * std::sin(t * 6.0f); // fast-ish blink

	for(int r = 0; r < rows; r++) {
		for(int c = 0; c < cols; c++) {
			if(c > 0) ImGui::SameLine();
			int slot = r * cols + c;
			ImGui::PushID(slot);

			bool has = snapshots.count(slot) > 0;
			bool act = (slot == currentSnapshotSlot);
			bool isBlinkingTarget = (isInterpolating && slot == interpolationTargetSlot);

			// Prepare label and replace /n with newlines
			std::string label = (has && showSnapshotNames.get())
								? snapshots[slot].name
								: ofToString(slot);
			
			std::string displayLabel = label;
			size_t pos = 0;
			while ((pos = displayLabel.find("/n", pos)) != std::string::npos) {
				displayLabel.replace(pos, 2, "\n");
				pos += 1;
			}

			// Determine button colors based on state
			ImVec4 buttonCol, buttonHoverCol, buttonActiveCol;
			if(isBlinkingTarget) {
				// orange-ish, blinking
				float baseR = 1.0f;
				float baseG = 0.5f;
				float baseB = 0.0f;
				buttonCol = ImVec4(baseR * (0.7f + 0.3f*blink), baseG * (0.5f + 0.5f*blink), baseB, 1.0f);
				buttonHoverCol = ImVec4(baseR, baseG, 0.0f, 1.0f);
				buttonActiveCol = ImVec4(baseR, baseG, 0.0f, 1.0f);
			}
			else if(act) {
				// Active snapshot - GREEN
				buttonCol = ImVec4(0,0.4f,0,1);
				buttonHoverCol = ImVec4(0,0.7f,0,1);
				buttonActiveCol = ImVec4(0,1,0,1);
			}
			else if(has) {
				// Stored snapshot - RED
				buttonCol = ImVec4(0.4f,0,0,1);
				buttonHoverCol = ImVec4(0.7f,0,0,1);
				buttonActiveCol = ImVec4(1,0,0,1);
			}
			else {
				buttonCol = ImVec4(0.2f,0.2f,0.2f,1);
				buttonHoverCol = ImVec4(0.3f,0.3f,0.3f,1);
				buttonActiveCol = ImVec4(0.4f,0.4f,0.4f,1);
			}

			// Use invisible button for interaction and custom rendering
			ImVec2 buttonSize_vec(buttonSize.get(), buttonSize.get()/1.5f);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			
			bool clicked = ImGui::InvisibleButton(("##btn" + ofToString(slot)).c_str(), buttonSize_vec);
			
			// Draw button background with appropriate color
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p1(p0.x + buttonSize_vec.x, p0.y + buttonSize_vec.y);
			
			ImVec4 currentCol = buttonCol;
			if(ImGui::IsItemActive()) {
				currentCol = buttonActiveCol;
			} else if(ImGui::IsItemHovered()) {
				currentCol = buttonHoverCol;
			}
			
			draw_list->AddRectFilled(p0, p1, ImGui::GetColorU32(currentCol), 3.0f);
			
			// Draw centered multi-line text
			ImVec2 textSize = ImGui::CalcTextSize(displayLabel.c_str());
			ImVec2 textPos(
				p0.x + (buttonSize_vec.x - textSize.x) * 0.5f,
				p0.y + (buttonSize_vec.y - textSize.y) * 0.5f
			);
			draw_list->AddText(textPos, ImGui::GetColorU32(ImVec4(0.8f,0.8f,0.8f,1)), displayLabel.c_str());
			
			if(clicked) {
				if(ImGui::GetIO().KeyShift) {
					storeSnapshot(slot);
				} else {
					if(interpolationMs.get() > 0) {
						startInterpolation(slot);
					} else {
						loadSnapshot(slot);
					}
				}
			}
			ImGui::PopID();
		}
	}
	
	ImGui::Text(". . . . . . . . . . . . . . . . .");
	ImGui::PopID();
}

void globalSnapshots::renderInspectorInterface() {
	if(ImGui::Button("Clear All Snapshots", ImVec2(140,0))) {
		snapshots.clear();
		currentSnapshotSlot = -1;
		saveSnapshotsToFile();
	}
	ImGui::Separator();

	if(snapshots.empty()) {
		ImGui::Text("No snapshots stored");
	} else {
		for(auto it = snapshots.begin(); it != snapshots.end(); ) {
			int slot = it->first;
			auto &sd = it->second;

			ImGui::PushID(slot);
			ImGui::Text("Slot %d", slot);
			ImGui::SameLine();

			char buf[64];
			std::strncpy(buf, sd.name.c_str(), sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			ImGui::SetNextItemWidth(120);
			if(ImGui::InputText("##name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
				sd.name = buf;
			}

			ImGui::SameLine();
			if(ImGui::Button("Load")) {
				loadSnapshot(slot);
			}

			ImGui::SameLine();
			if(ImGui::Button("Clear")) {
				it = snapshots.erase(it);
				if(currentSnapshotSlot == slot) currentSnapshotSlot = -1;
				ImGui::PopID();
				continue;
			}

			ImGui::PopID();
			++it;
		}
	}

	ImGui::Separator();
	ImGui::Text("Parameter Excludes");
	ImGui::TextDisabled("These won't be stored / recalled / interpolated.");
	
	if(!globalContainer) {
		ImGui::TextDisabled("No container");
		return;
	}

	static char filter[64] = "";
	ImGui::InputText("Filter", filter, sizeof(filter));

	for(auto *node : globalContainer->getAllModules()) {
		if(!node) continue;
		auto &grp = node->getParameters();
		std::string grpName = grp.getEscapedName();

		if(ImGui::TreeNode(grpName.c_str())) {
			for(int i = 0; i < grp.size(); ++i) {
				auto &p = grp.get(i);
				auto *oParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&p);
				std::string key = grpName + "/" + p.getName();

				if(filter[0] != '\0') {
					if(key.find(filter) == std::string::npos) continue;
				}

				bool isExcluded = (manualExcludes.find(key) != manualExcludes.end());
				bool isOutputFlagged = false;
				if(oParam) {
					auto flags = oParam->getFlags();
					if(flags & ofxOceanodeParameterFlags_DisableInConnection) {
						isOutputFlagged = true;
					}
				}

				if(isOutputFlagged) {
					ImGui::BeginDisabled();
					ImGui::Checkbox((key + " (output)").c_str(), &isExcluded);
					ImGui::EndDisabled();
				} else {
					if(ImGui::Checkbox(key.c_str(), &isExcluded)) {
						if(isExcluded)
							manualExcludes.insert(key);
						else
							manualExcludes.erase(key);
						saveSnapshotsToFile();
					}
				}
			}
			ImGui::TreePop();
		}
	}
}

std::string globalSnapshots::getSnapshotsFilePath() {
	std::string canvasPath = getParents();
	if(canvasPath.empty()) {
		return ofToDataPath("globalSnapshots.json", true);
	} else {
		std::string dirPath = ofToDataPath("Snapshots/" + canvasPath, true);
		ofDirectory::createDirectory(dirPath, true, true);
		return dirPath + "/globalSnapshots.json";
	}
}

void globalSnapshots::saveSnapshotsToFile() {
	if(snapshots.empty() && manualExcludes.empty()) {
		std::string filePath = getSnapshotsFilePath();
		if(ofFile::doesFileExist(filePath)) {
			ofFile::removeFile(filePath);
		}
		return;
	}
	
	ofJson json;
	for(const auto& pair : snapshots) {
		ofJson snapshotJson;
		snapshotJson["name"] = pair.second.name;
		
		ofJson paramsJson;
		for(const auto& paramPair : pair.second.paramValues) {
			ofJson paramJson;
			paramJson["type"] = paramPair.second.type;
			paramJson["value"] = paramPair.second.value;
			paramsJson[paramPair.first] = paramJson;
		}
		snapshotJson["parameters"] = paramsJson;
		
		json[ofToString(pair.first)] = snapshotJson;
	}

	ofJson excludedJson = ofJson::array();
	for(const auto &k : manualExcludes) {
		excludedJson.push_back(k);
	}
	json["_excluded"] = excludedJson;
	
	std::string filePath = getSnapshotsFilePath();
	if(ofSavePrettyJson(filePath, json)) {
		ofLogNotice("globalSnapshots") << "Saved snapshots to: " << filePath;
	} else {
		ofLogError("globalSnapshots") << "Failed to save snapshots to: " << filePath;
	}
}

void globalSnapshots::loadSnapshotsFromFile() {
	std::string filePath = getSnapshotsFilePath();
	
	if(!ofFile::doesFileExist(filePath)) {
		ofLogVerbose("globalSnapshots") << "No snapshots file found at: " << filePath;
		return;
	}
	
	try {
		ofJson json = ofLoadJson(filePath);
		snapshots.clear();
		manualExcludes.clear();
		
		for(auto it = json.begin(); it != json.end(); ++it) {
			if(it.key() == "_excluded") {
				continue;
			}

			int slot = ofToInt(it.key());
			SnapshotData snapshotData;
			
			if(it.value().contains("name")) {
				snapshotData.name = it.value()["name"].get<std::string>();
			} else {
				snapshotData.name = ofToString(slot);
			}
			
			if(it.value().contains("parameters")) {
				for(auto paramIt = it.value()["parameters"].begin();
					paramIt != it.value()["parameters"].end(); ++paramIt) {
					
					ParameterSnapshot paramSnapshot;
					paramSnapshot.type = paramIt.value()["type"].get<std::string>();
					paramSnapshot.value = paramIt.value()["value"];
					
					snapshotData.paramValues[paramIt.key()] = paramSnapshot;
				}
			}
			
			snapshots[slot] = snapshotData;
		}

		if(json.contains("_excluded") && json["_excluded"].is_array()) {
			for(auto &v : json["_excluded"]) {
				manualExcludes.insert(v.get<std::string>());
			}
		}
		
		ofLogNotice("globalSnapshots") << "Loaded " << snapshots.size() << " snapshots from: " << filePath
									   << " and " << manualExcludes.size() << " excluded params";
		
	} catch(const std::exception& e) {
		ofLogError("globalSnapshots") << "Error loading snapshots: " << e.what();
		snapshots.clear();
		manualExcludes.clear();
	}
}

void globalSnapshots::startInterpolation(int targetSlot) {
	if (!globalContainer) return;
	
	auto it = snapshots.find(targetSlot);
	if(it == snapshots.end()) return;
	
	interpolationStartValues.clear();
	interpolationActiveKeys.clear();

	auto &targetSnap = it->second.paramValues;
	
	// Extract BiPow value from target snapshot to use for this interpolation
	interpolationBiPowValue = 0.0f; // default
	
	// BiPow is stored with the module's instance name (e.g., "Global_Snapshots_1/BiPow")
	// The slot number is at the JSON top level, not in the parameter key
	std::string expectedKey = "Global_Snapshots_1/BiPow";
	
	auto tgtIt = targetSnap.find(expectedKey);
	
	if(tgtIt != targetSnap.end()) {
		try {
			if(tgtIt->second.value.is_number()) {
				interpolationBiPowValue = tgtIt->second.value.get<float>();
			}
		} catch(...) {
			interpolationBiPowValue = 0.0f;
		}
	}
	
	for(auto *node : globalContainer->getAllModules()) {
		if (!node) continue;
		
		if(!includeMacroParams.get() && dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
			continue;
		}
		
		auto &grp = node->getParameters();
		std::string grpName = grp.getEscapedName();

		for(int i = 0; i < grp.size(); i++) {
			auto &p = grp.get(i);
			auto *oParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&p);
			if(!oParam) continue;

			std::string key = grpName + "/" + p.getName();
			auto tgtIt = targetSnap.find(key);
			if(tgtIt == targetSnap.end()) {
				continue;
			}

			if(isParameterExcluded(key, oParam)) {
				continue;
			}

			// If this parameter should not be interpolated, apply it instantly
			if(shouldSkipInterpolation(key)) {
				try {
					auto &ps = tgtIt->second;
					if(ps.type == typeid(float).name()) {
						oParam->cast<float>().getParameter() = ps.value.get<float>();
					}
					else if(ps.type == typeid(int).name()) {
						oParam->cast<int>().getParameter() = (int)std::round(ps.value.get<float>());
					}
				} catch(...) {
					// Skip
				}
				continue; // Don't add to interpolation
			}

			ParameterSnapshot ps;
			ps.type = oParam->valueType();

			try {
				bool differs = false;

				if(ps.type == typeid(float).name()) {
					float cur = oParam->cast<float>().getParameter().get();
					float tgt = tgtIt->second.value.get<float>();
					ps.value = cur;
					if(!floatsEqual(cur, tgt)) differs = true;
				}
				else if(ps.type == typeid(int).name()) {
					int cur = oParam->cast<int>().getParameter().get();
					int tgt = (int)std::round(tgtIt->second.value.get<float>());
					ps.value = (float)cur;
					if(cur != tgt) differs = true;
				}
				else if(ps.type == typeid(std::vector<float>).name()) {
					auto cur = oParam->cast<std::vector<float>>().getParameter().get();
					auto tgt = tgtIt->second.value.get<std::vector<float>>();
					ps.value = cur;
					if(!floatVectorsEqual(cur, tgt)) differs = true;
				}
				else if(ps.type == typeid(std::vector<int>).name()) {
					auto curI = oParam->cast<std::vector<int>>().getParameter().get();
					std::vector<float> curF(curI.begin(), curI.end());
					auto tgtF = tgtIt->second.value.get<std::vector<float>>();
					ps.value = curF;
					if(curF.size() != tgtF.size()) {
						differs = true;
					} else {
						for(size_t k = 0; k < curF.size(); ++k) {
							if(std::fabs(curF[k] - tgtF[k]) > 1e-6f) {
								differs = true;
								break;
							}
						}
					}
				}
				else {
					differs = true;
					if(ps.type == typeid(bool).name()) {
						ps.value = oParam->cast<bool>().getParameter().get();
					} else if(ps.type == typeid(std::string).name()) {
						ps.value = oParam->cast<std::string>().getParameter().get();
					}
				}

				interpolationStartValues[key] = ps;

				if(differs) {
					interpolationActiveKeys.insert(key);
				}
			} catch(...) {
				// Skip
			}
		}
	}
	
	isInterpolating = true;
	interpolationStartTime = ofGetElapsedTimeMillis();
	interpolationTargetSlot = targetSlot;
	transition = 0.0f;
}

void globalSnapshots::updateInterpolation() {
	if (!isInterpolating || !globalContainer) return;
	
	float currentTime = ofGetElapsedTimeMillis();
	float elapsed = currentTime - interpolationStartTime;
	float progress = interpolationMs.get() > 0 ? (elapsed / interpolationMs.get()) : 1.0f;
	
	if (progress >= 1.0f) {
		progress = 1.0f;
		isInterpolating = false;
		currentSnapshotSlot = interpolationTargetSlot;
		done.trigger();
	}
	
	// Apply BiPow to the progress for easing (use stored value from when interpolation started)
	float easedProgress = progress;
	
	if(interpolationBiPowValue != 0.0f){
		easedProgress = (easedProgress*2.0f) - 1.0f;  // Map [0,1] to [-1,1]
		customPow(easedProgress, interpolationBiPowValue);
		easedProgress = (easedProgress+1.0f) * 0.5f;  // Map [-1,1] back to [0,1]
	} else {
		// When BiPow is 0, apply smoothstep instead for smooth easing
		easedProgress = easedProgress * easedProgress * (3.0f - 2.0f * easedProgress);
	}
	
	// Update transition parameter with the eased progress
	transition = easedProgress;
	
	auto targetSnapshot = snapshots.find(interpolationTargetSlot);
	if(targetSnapshot == snapshots.end()) {
		isInterpolating = false;
		return;
	}
	
	for (const auto& key : interpolationActiveKeys) {
		for (auto *node : globalContainer->getAllModules()) {
			if (!node) continue;
			if(!includeMacroParams.get() && dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
				continue;
			}
			
			auto &grp = node->getParameters();
			std::string grpName = grp.getEscapedName();
			
			for (int i = 0; i < grp.size(); i++) {
				auto &p = grp.get(i);
				auto *oParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&p);
				if(!oParam) continue;
				
				std::string thisKey = grpName + "/" + p.getName();
				if (thisKey != key) continue;
				
				auto startIt  = interpolationStartValues.find(key);
				auto targetIt = targetSnapshot->second.paramValues.find(key);
				if(startIt == interpolationStartValues.end() || targetIt == targetSnapshot->second.paramValues.end())
					break;
				
				try {
					auto vtype = oParam->valueType();
					if(vtype == typeid(float).name()) {
						float startVal  = startIt->second.value.get<float>();
						float targetVal = targetIt->second.value.get<float>();
						float interpolatedVal = startVal + (targetVal - startVal) * easedProgress;
						oParam->cast<float>().getParameter() = interpolatedVal;
					}
					else if(vtype == typeid(int).name()) {
						float startVal  = startIt->second.value.get<float>();
						float targetVal = targetIt->second.value.get<float>();
						float interpolatedVal = startVal + (targetVal - startVal) * easedProgress;
						oParam->cast<int>().getParameter() = static_cast<int>(std::round(interpolatedVal));
					}
					else if(vtype == typeid(std::vector<float>).name()) {
						auto startVec  = startIt->second.value.get<std::vector<float>>();
						auto targetVec = targetIt->second.value.get<std::vector<float>>();
						if(startVec.size() == targetVec.size()) {
							std::vector<float> interpolatedVec;
							interpolatedVec.reserve(startVec.size());
							for(size_t j = 0; j < startVec.size(); j++) {
								float interpolatedVal = startVec[j] + (targetVec[j] - startVec[j]) * easedProgress;
								interpolatedVec.push_back(interpolatedVal);
							}
							oParam->cast<std::vector<float>>().getParameter() = interpolatedVec;
						}
					}
					else if(vtype == typeid(std::vector<int>).name()) {
						auto startVec  = startIt->second.value.get<std::vector<float>>();
						auto targetVec = targetIt->second.value.get<std::vector<float>>();
						if(startVec.size() == targetVec.size()) {
							std::vector<int> interpolatedVec;
							interpolatedVec.reserve(startVec.size());
							for(size_t j = 0; j < startVec.size(); j++) {
								float interpolatedVal = startVec[j] + (targetVec[j] - startVec[j]) * easedProgress;
								interpolatedVec.push_back(static_cast<int>(std::round(interpolatedVal)));
							}
							oParam->cast<std::vector<int>>().getParameter() = interpolatedVec;
						}
					}
					else {
						if(progress >= 0.5f) {
							if(vtype == typeid(bool).name()) {
								oParam->cast<bool>().getParameter() = targetIt->second.value.get<bool>();
							} else if(vtype == typeid(std::string).name()) {
								oParam->cast<std::string>().getParameter() = targetIt->second.value.get<std::string>();
							}
						}
					}
				} catch(...) {
					// Skip
				}
				
				break;
			}
		}
	}
}
