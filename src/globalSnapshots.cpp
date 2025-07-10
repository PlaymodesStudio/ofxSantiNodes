// globalSnapshots.cpp
#include "globalSnapshots.h"
#include "imgui.h"
#include "ofxOceanodeNodeMacro.h"  // for dynamic_cast check
#include <vector>
#include <cmath>

globalSnapshots::globalSnapshots()
: ofxOceanodeNodeModel("Global Snapshots")
, currentSnapshotSlot(-1)
, globalContainer(nullptr)
, isInterpolating(false)
, interpolationStartTime(0)
, interpolationTargetSlot(-1)
{
}

void globalSnapshots::setContainer(ofxOceanodeContainer* c) {
	globalContainer = c;
	// Let base class also store internally (for any future use)
	ofxOceanodeNodeModel::setContainer(c);
}

void globalSnapshots::setup() {
	setup("");
}

void globalSnapshots::setup(string additionalInfo) {
	description = "Global snapshot system that captures all parameters in the current canvas. Shift+click to store, click to recall.";
	
	// Initialize default values for parameters
	matrixRows.set("Rows", 2, 1, 8);
	matrixCols.set("Cols", 8, 1, 8);
	buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f);
	showSnapshotNames.set("Show Names", true);
	includeMacroParams.set("Include Macro Params", false);
	interpolationMs.set("Interpolation Ms", 0.0f, 0.0f, 5000.0f);
	
	// Inspector parameters - these appear in the inspector panel
	addInspectorParameter(includeMacroParams);
	addInspectorParameter(matrixRows);
	addInspectorParameter(matrixCols);
	addInspectorParameter(buttonSize);
	addInspectorParameter(showSnapshotNames);

	// "Add Snapshot" button in inspector
	addInspectorParameter(addSnapshotButton.set("Add Snapshot"));
	addSnapshotListener = addSnapshotButton.newListener([this](){
		int newSlot = snapshots.empty() ? 0 : (snapshots.rbegin()->first + 1);
		storeSnapshot(newSlot);
	});

	// Main node parameters - these appear in the main node interface
	addParameter(interpolationMs);
	addParameter(activeSnapshotSlot.set(
		"Slot",
		-1,                                    // default = "none"
		-1,                                    // min = "none"
		matrixRows.get() * matrixCols.get() - 1 // max
	));
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int &slot){
		if(slot >= 0) {
			if(interpolationMs.get() > 0) {
				startInterpolation(slot);
			} else {
				loadSnapshot(slot);
			}
		}
	});

	// Node GUI: the button matrix itself - using addCustomRegion
	addCustomRegion(snapshotControlGui.set("Snapshots", [this](){
		renderSnapshotMatrix();
	}), [this](){
		renderSnapshotMatrix();
	});

	// Inspector: rename / clear interface - using addInspectorParameter
	addInspectorParameter(snapshotInspector.set("Snapshot Names", [this](){
		renderInspectorInterface();
	}));
	
	// Load existing snapshots from file
	loadSnapshotsFromFile();
}

void globalSnapshots::update(ofEventArgs &e) {
	if(isInterpolating) {
		updateInterpolation();
	}
}

void globalSnapshots::storeSnapshot(int slot) {
	if (!globalContainer) {
		ofLogError("globalSnapshots") << "No global container set";
		return;
	}
	
	SnapshotData data;
	// Preserve existing name if re‐storing
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		data.name = it->second.name;
	} else {
		data.name = ofToString(slot);
	}

	int paramCount = 0;
	// Walk *all* parameters in the patch
	for(auto *node : globalContainer->getAllModules()) {
		if (!node) continue;
		
		// Optionally skip macros
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
			ParameterSnapshot ps;
			ps.type = oParam->valueType();

			try {
				// common types
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
	
	// Save to file immediately
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

			auto &ps = pit->second;
			try {
				if(ps.type == typeid(float).name()) {
					oParam->cast<float>().getParameter() = ps.value.get<float>();
					loadedCount++;
				}
				else if(ps.type == typeid(int).name()) {
					oParam->cast<int>().getParameter() = (int)std::round(ps.value.get<float>());
					loadedCount++;
				}
				else if(ps.type == typeid(bool).name()) {
					oParam->cast<bool>().getParameter() = ps.value.get<bool>();
					loadedCount++;
				}
				else if(ps.type == typeid(std::string).name()) {
					oParam->cast<std::string>().getParameter() = ps.value.get<std::string>();
					loadedCount++;
				}
				else if(ps.type == typeid(std::vector<float>).name()) {
					oParam->cast<std::vector<float>>().getParameter() = ps.value.get<std::vector<float>>();
					loadedCount++;
				}
				else if(ps.type == typeid(std::vector<int>).name()) {
					auto vf = ps.value.get<std::vector<float>>();
					std::vector<int> vi;
					for(auto f : vf) vi.push_back((int)std::round(f));
					oParam->cast<std::vector<int>>().getParameter() = vi;
					loadedCount++;
				}
			} catch(...) {
				ofLogWarning("globalSnapshots") << "Failed to load parameter: " << key;
			}
		}
	}

	currentSnapshotSlot = slot;
	ofLogNotice("globalSnapshots") << "Loaded snapshot " << slot << " - " << loadedCount << " parameters restored";
}

void globalSnapshots::renderSnapshotMatrix() {
	ImGui::PushID("GlobalSnapshots");
	int rows = matrixRows.get();
	int cols = matrixCols.get();

	for(int r = 0; r < rows; r++) {
		for(int c = 0; c < cols; c++) {
			if(c > 0) ImGui::SameLine();
			int slot = r * cols + c;
			ImGui::PushID(slot);

			bool has = snapshots.count(slot) > 0;
			bool act = (slot == currentSnapshotSlot);

			// Color coding
			if(act) {
				ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.4f,0,0,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.7f,0,0,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,0,0,1));
			}
			else if(has) {
				ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.2f,0.4f,0,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.25f,0.5f,0,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f,0.6f,0,1));
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.2f,0.2f,0.2f,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.3f,0.3f,0.3f,1));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f,0.4f,0.4f,1));
			}
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f,0.8f,0.8f,1));

			std::string label = (has && showSnapshotNames.get())
								? snapshots[slot].name
								: ofToString(slot);

			if(ImGui::Button(label.c_str(), ImVec2(buttonSize.get(), buttonSize.get()/1.5f))) {
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

			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}
	}
	
	// Add a footer to ensure the node has some content
	ImGui::Text(". . . . . . . . . . . . . . . . .");
	ImGui::PopID();
}

void globalSnapshots::renderInspectorInterface() {
	// Clear All
	if(ImGui::Button("Clear All Snapshots", ImVec2(140,0))) {
		snapshots.clear();
		currentSnapshotSlot = -1;
		saveSnapshotsToFile(); // Save after clearing
	}
	ImGui::Separator();

	if(snapshots.empty()) {
		ImGui::Text("No snapshots stored");
		return;
	}

	for(auto it = snapshots.begin(); it != snapshots.end(); ) {
		int slot = it->first;
		auto &sd = it->second;

		ImGui::PushID(slot);
		ImGui::Text("Slot %d", slot);
		ImGui::SameLine();

		// Rename field
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
			continue;  // skip ++it
		}

		ImGui::PopID();
		++it;
	}
}

string globalSnapshots::getSnapshotsFilePath() {
	// Use the canvas ID to create a unique file path
	string canvasPath = getParents();
	if(canvasPath.empty()) {
		return ofToDataPath("globalSnapshots.json", true);
	} else {
		// Create directory if it doesn't exist
		string dirPath = ofToDataPath("Snapshots/" + canvasPath, true);
		ofDirectory::createDirectory(dirPath, true, true);
		return dirPath + "/globalSnapshots.json";
	}
}

void globalSnapshots::saveSnapshotsToFile() {
	if(snapshots.empty()) {
		// Remove file if no snapshots
		string filePath = getSnapshotsFilePath();
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
	
	string filePath = getSnapshotsFilePath();
	if(ofSavePrettyJson(filePath, json)) {
		ofLogNotice("globalSnapshots") << "Saved snapshots to: " << filePath;
	} else {
		ofLogError("globalSnapshots") << "Failed to save snapshots to: " << filePath;
	}
}

void globalSnapshots::loadSnapshotsFromFile() {
	string filePath = getSnapshotsFilePath();
	
	if(!ofFile::doesFileExist(filePath)) {
		ofLogVerbose("globalSnapshots") << "No snapshots file found at: " << filePath;
		return;
	}
	
	try {
		ofJson json = ofLoadJson(filePath);
		snapshots.clear();
		
		for(auto it = json.begin(); it != json.end(); ++it) {
			int slot = ofToInt(it.key());
			SnapshotData snapshotData;
			
			if(it.value().contains("name")) {
				snapshotData.name = it.value()["name"].get<string>();
			} else {
				snapshotData.name = ofToString(slot);
			}
			
			if(it.value().contains("parameters")) {
				for(auto paramIt = it.value()["parameters"].begin();
					paramIt != it.value()["parameters"].end(); ++paramIt) {
					
					ParameterSnapshot paramSnapshot;
					paramSnapshot.type = paramIt.value()["type"].get<string>();
					paramSnapshot.value = paramIt.value()["value"];
					
					snapshotData.paramValues[paramIt.key()] = paramSnapshot;
				}
			}
			
			snapshots[slot] = snapshotData;
		}
		
		ofLogNotice("globalSnapshots") << "Loaded " << snapshots.size() << " snapshots from: " << filePath;
		
	} catch(const std::exception& e) {
		ofLogError("globalSnapshots") << "Error loading snapshots: " << e.what();
		snapshots.clear();
	}
}

void globalSnapshots::startInterpolation(int targetSlot) {
	if (!globalContainer) return;
	
	auto it = snapshots.find(targetSlot);
	if(it == snapshots.end()) return;
	
	// Capture current parameter values as start values
	interpolationStartValues.clear();
	
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
			ParameterSnapshot ps;
			ps.type = oParam->valueType();

			try {
				// Capture current values for interpolation start
				if(ps.type == typeid(float).name()) {
					ps.value = oParam->cast<float>().getParameter().get();
				}
				else if(ps.type == typeid(int).name()) {
					ps.value = static_cast<float>(oParam->cast<int>().getParameter().get());
				}
				else if(ps.type == typeid(std::vector<float>).name()) {
					ps.value = oParam->cast<std::vector<float>>().getParameter().get();
				}
				else if(ps.type == typeid(std::vector<int>).name()) {
					auto vi = oParam->cast<std::vector<int>>().getParameter().get();
					std::vector<float> vf(vi.begin(), vi.end());
					ps.value = vf;
				}
				// Note: bool and string types will snap immediately (no interpolation)
				
				interpolationStartValues[key] = ps;
			} catch(...) {
				// Skip parameters that can't be captured
			}
		}
	}
	
	// Start interpolation
	isInterpolating = true;
	interpolationStartTime = ofGetElapsedTimeMillis();
	interpolationTargetSlot = targetSlot;
	
	ofLogNotice("globalSnapshots") << "Started interpolation to slot " << targetSlot << " over " << interpolationMs.get() << "ms";
}

void globalSnapshots::updateInterpolation() {
	if (!isInterpolating || !globalContainer) return;
	
	float currentTime = ofGetElapsedTimeMillis();
	float elapsed = currentTime - interpolationStartTime;
	float progress = elapsed / interpolationMs.get();
	
	if (progress >= 1.0f) {
		// Interpolation complete - snap to final values
		progress = 1.0f;
		isInterpolating = false;
		currentSnapshotSlot = interpolationTargetSlot;
		ofLogNotice("globalSnapshots") << "Interpolation complete";
	}
	
	// Apply eased interpolation (ease in/out)
	float easedProgress = progress * progress * (3.0f - 2.0f * progress); // smoothstep
	
	auto targetSnapshot = snapshots.find(interpolationTargetSlot);
	if(targetSnapshot == snapshots.end()) {
		isInterpolating = false;
		return;
	}
	
	// Apply interpolated values
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
			
			auto startIt = interpolationStartValues.find(key);
			auto targetIt = targetSnapshot->second.paramValues.find(key);
			
			if(startIt == interpolationStartValues.end() || targetIt == targetSnapshot->second.paramValues.end()) {
				continue;
			}
			
			try {
				if(oParam->valueType() == typeid(float).name()) {
					float startVal = startIt->second.value.get<float>();
					float targetVal = targetIt->second.value.get<float>();
					float interpolatedVal = startVal + (targetVal - startVal) * easedProgress;
					oParam->cast<float>().getParameter() = interpolatedVal;
				}
				else if(oParam->valueType() == typeid(int).name()) {
					float startVal = startIt->second.value.get<float>();
					float targetVal = targetIt->second.value.get<float>();
					float interpolatedVal = startVal + (targetVal - startVal) * easedProgress;
					oParam->cast<int>().getParameter() = static_cast<int>(std::round(interpolatedVal));
				}
				else if(oParam->valueType() == typeid(std::vector<float>).name()) {
					auto startVec = startIt->second.value.get<std::vector<float>>();
					auto targetVec = targetIt->second.value.get<std::vector<float>>();
					
					if(startVec.size() == targetVec.size()) {
						std::vector<float> interpolatedVec;
						for(size_t j = 0; j < startVec.size(); j++) {
							float interpolatedVal = startVec[j] + (targetVec[j] - startVec[j]) * easedProgress;
							interpolatedVec.push_back(interpolatedVal);
						}
						oParam->cast<std::vector<float>>().getParameter() = interpolatedVec;
					}
				}
				else if(oParam->valueType() == typeid(std::vector<int>).name()) {
					auto startVec = startIt->second.value.get<std::vector<float>>();
					auto targetVec = targetIt->second.value.get<std::vector<float>>();
					
					if(startVec.size() == targetVec.size()) {
						std::vector<int> interpolatedVec;
						for(size_t j = 0; j < startVec.size(); j++) {
							float interpolatedVal = startVec[j] + (targetVec[j] - startVec[j]) * easedProgress;
							interpolatedVec.push_back(static_cast<int>(std::round(interpolatedVal)));
						}
						oParam->cast<std::vector<int>>().getParameter() = interpolatedVec;
					}
				}
				else {
					// For bool, string, etc. - snap immediately at 50% progress
					if(progress >= 0.5f) {
						if(oParam->valueType() == typeid(bool).name()) {
							oParam->cast<bool>().getParameter() = targetIt->second.value.get<bool>();
						}
						else if(oParam->valueType() == typeid(std::string).name()) {
							oParam->cast<std::string>().getParameter() = targetIt->second.value.get<std::string>();
						}
					}
				}
			} catch(...) {
				// Skip parameters that can't be interpolated
			}
		}
	}
}
