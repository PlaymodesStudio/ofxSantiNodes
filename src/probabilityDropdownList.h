#ifndef probabilityDropdownList_h
#define probabilityDropdownList_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>

// Structure to hold snapshot data
struct DropdownListSnapshot {
	string name;
	int bars;
	int notegrid;
	int bpm;
	int everyN;
	int root;
	int palette;
	int scale;
	vector<float> transposeWeights;
	vector<float> degreeWeights;
	vector<vector<vector<float>>> vectorValues;
	vector<pair<string, string>> cellContents;
	vector<int> probabilities;
	vector<int> groups;
};

class probabilityDropdownList : public ofxOceanodeNodeModel {
public:
	probabilityDropdownList() : ofxOceanodeNodeModel("FORMS P5 PNG Director") {
		description = "Defines the properties to be sent to Processing for PNG generation";
		
		// Initialize options
		initializeOptions();
		
		// Initialize vectors with correct sizes and bounds
		vector<float> initWeights(12, 0.0f);
		vector<float> minWeights(12, 0.0f);
		vector<float> maxWeights(12, 1.0f);
		
		vector<int> initOutput(16, 0);
		vector<int> minOutput(16, 0);
		vector<int> maxOutput(16, 11);
		
		// Add parameters in specific order matching original implementation
		addParameter(bars.set("Bars", 8, 1, 32));
		addParameter(notegrid.set("Notegrid", 100, 1, 130));
		addParameter(bpm.set("BPM", 120, 20, 999));
		addParameter(everyN.set("EveryN", 1, 1, 16));
		addParameter(root.set("Root", 24, 0, 127));
		
		// Add dropdown parameters
		addParameterDropdown(palette, "Palette", 0, paletteOptions);
		addParameterDropdown(scale, "Scale", 0, scaleOptions);
		
		// Add vector parameters with explicit bounds
		addParameter(transposeWeights.set("TransWeights", initWeights, minWeights, maxWeights),
			ofxOceanodeParameterFlags_DisplayMinimized);
		addParameter(degreeWeights.set("DegWeights", initWeights, minWeights, maxWeights),
			ofxOceanodeParameterFlags_DisplayMinimized);
		
		// Add output parameters with explicit bounds
		addOutputParameter(transposeOut.set("TransposeOut", initOutput, minOutput, maxOutput));
		addOutputParameter(degreeOut.set("DegreeOut", initOutput, minOutput, maxOutput));

		// Add original parameters with proper initialization
		addParameter(elementsPath.set("Path", ""));
		addParameter(browseButton.set("Open"));
		addParameter(widgetWidth.set("Width", 360, 100, 500));
		addParameter(numRows.set("Rows", 8, 1, 64));
		addParameter(globalProb.set("Global %", 100, 0, 100));
		
		// Initialize groupProb with proper bounds
		vector<float> initGroupProb(1, 1.0f);
		vector<float> minGroupProb(1, 0.0f);
		vector<float> maxGroupProb(1, 1.0f);
		addParameter(groupProb.set("Group %", initGroupProb, minGroupProb, maxGroupProb));
		
		// Add snapshot-related parameters
		vector<string> initialOptions = {"None"};
		addParameter(activeSnapshotSlot.set("Snapshot", 0, 0, 63));
		addInspectorParameter(showSnapshotNames.set("Show Names", true));
		addInspectorParameter(addSnapshotButton.set("Add Snapshot"));
		addInspectorParameter(snapshotInspector.set("Snapshot Names", [this]() {
			renderSnapshotInspector();
		}));
		
		addParameter(trigger.set("GO"));
		addOutputParameter(output.set("Output", ""));

		// Initialize internal vectors with safe sizes
		cellContents.resize(64);
		probabilities.resize(64, 0);
		groups.resize(64, 0);
		lastResults.resize(64, false);
		
		vectorValues.resize(64);
		for(auto &slot : vectorValues) {
			slot.resize(numSliders);
			for(auto &sliderValues : slot) {
				sliderValues.resize(10, 0.0f);
			}
		}
		
		vectorValueParams.resize(numSliders);
		currentToEditValues.resize(numSliders);
		
		// Add custom region for GUI
		addCustomRegion(guiRegion, [this](){
			drawGui();
		});
		
		// Setup listeners
		setupListeners();
		
		// Initialize snapshot system
		initializeSnapshotSystem();
	}

	void initializeOptions() {
		rhythmOptions = {"none"};
		textureOptions = {"none"};
		harmonyOptions = {"none"};
		fxOptions = {"none"};
		
		paletteOptions = {
			"red yellow", "blue yellow", "brown blue", "pink brown", "white",
			"green blue", "orange violet", "warm", "whitered", "whiteredcyan",
			"whiteyellow", "whiteblue", "whittegreen", "whitepink", "whiteorange",
			"whiteviolet", "whitevioletcyan", "bauhaus"
		};
		
		scaleOptions = {
			"japanese", "minor pentatonic", "hirajoshi", "diminished", "wholetone",
			"arabian", "minor chord", "mM7", "M7", "m9", "m7", "chromatic",
			"mongolia", "FORMS_Fa", "FORMS_Fa_extended", "eolian", "dorian",
			"ionian", "majorChord", "M9", "arabian2", "2-1-4-2-1", "arabian3",
			"M7#9", "m6", "dysklavier_sync", "diminished"
		};
	}
	
	void setupListeners() {
		listeners.push(trigger.newListener([this]() {
			generateOutput();
			generateWeightedOutputs();
		}));
		
		listeners.push(browseButton.newListener([this]() {
			ofFileDialogResult result = ofSystemLoadDialog("Select elements file", false);
			if(result.bSuccess) {
				elementsPath = result.getPath();
				loadElementsFromFile(elementsPath);
			}
		}));
		
		listeners.push(elementsPath.newListener([this](string &path){
			loadElementsFromFile(path);
		}));
		
		listeners.push(bars.newListener([this](int &n) {
			updateVectorSizes();
		}));
		
		listeners.push(activeSnapshotSlot.newListener([this](int& slot){
			ofLogNotice("SnapshotDropdown") << "Loading snapshot slot: " << slot;
			loadSnapshot(slot);
		}));

		// Keep this part the same
		snapshotListeners.push(addSnapshotButton.newListener([this](){
			int newSlot = snapshots.empty() ? 0 : (--snapshots.end())->first + 1;
			storeSnapshot(newSlot);
		}));
	}

	void presetSave(ofJson &json) override {
		   // Save original data
		   for (int slot = 0; slot < 64; slot++) {
			   for (int slider = 0; slider < vectorValues[slot].size(); slider++) {
				   json["Values"][ofToString(slot)][ofToString(slider)] = vectorValues[slot][slider];
			   }
		   }
		   
		   // Save snapshots
		   for(const auto& pair : snapshots) {
			   ofJson snapshotJson;
			   saveSnapshotToJson(pair.second, snapshotJson);
			   json["Snapshots"][ofToString(pair.first)] = snapshotJson;
		   }
	   }

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if (json.count("Values") == 1) {
			string cellDataStr = json["cellData"];
			string probDataStr = json["probData"];
			string groupDataStr = json["groupData"];
			numRows = json["numRows"];
			
			vector<string> cellTokens = ofSplitString(cellDataStr, "|");
			vector<string> probTokens = ofSplitString(probDataStr, "|");
			vector<string> groupTokens = ofSplitString(groupDataStr, "|");
			
			cellContents.clear();
			probabilities.clear();
			groups.clear();
			
			for(int i = 0; i < cellTokens.size(); i++) {
				vector<string> cellParts = ofSplitString(cellTokens[i], ":");
				if(cellParts.size() == 2) {
					cellContents.push_back(make_pair(cellParts[0], cellParts[1]));
					probabilities.push_back(ofToInt(probTokens[i]));
					groups.push_back(ofToInt(groupTokens[i]));
				}
			}
			
			// Resize to match numRows
			cellContents.resize(numRows);
			probabilities.resize(numRows);
			groups.resize(numRows);
			lastResults.resize(numRows);
		}
		
		if(json.contains("Snapshots")) {
				snapshots.clear();
				for(auto& [key, value] : json["Snapshots"].items()) {
					int slot = ofToInt(key);
					DropdownListSnapshot snapshot;
					loadSnapshotFromJson(snapshot, value);
					snapshots[slot] = snapshot;
				}
			}
	}

private:
	
	void initializeSnapshotSystem() {
			currentSnapshotSlot = -1;
			snapshots.clear();
		}
	
		
	void storeSnapshot(int slot) {
			DropdownListSnapshot snapshot;
		// Preserve existing name if updating
		   auto existingSnapshot = snapshots.find(slot);
		   if(existingSnapshot != snapshots.end()) {
			   snapshot.name = existingSnapshot->second.name;
		   } else {
			   snapshot.name = "Snapshot " + ofToString(slot);
		   }
			
			// Capture all parameter values
			snapshot.bars = bars;
			snapshot.notegrid = notegrid;
			snapshot.bpm = bpm;
			snapshot.everyN = everyN;
			snapshot.root = root;
			snapshot.palette = palette;
			snapshot.scale = scale;
			snapshot.transposeWeights = transposeWeights;
			snapshot.degreeWeights = degreeWeights;
			snapshot.vectorValues = vectorValues;
			snapshot.cellContents = cellContents;
			snapshot.probabilities = probabilities;
			snapshot.groups = groups;
			
			snapshots[slot] = snapshot;
			currentSnapshotSlot = slot;

		}
		
	void loadSnapshot(int slot) {
		ofLogNotice("SnapshotDropdown") << "Loading snapshot slot " << slot;
		
		auto it = snapshots.find(slot);
		if(it == snapshots.end()) {
			ofLogError("SnapshotDropdown") << "Snapshot not found";
			return;
		}
		
		const auto& snapshot = it->second;
		
		// Update all values
		bars = snapshot.bars;
		notegrid = snapshot.notegrid;
		bpm = snapshot.bpm;
		everyN = snapshot.everyN;
		root = snapshot.root;
		palette = snapshot.palette;
		scale = snapshot.scale;
		transposeWeights = snapshot.transposeWeights;
		degreeWeights = snapshot.degreeWeights;
		vectorValues = snapshot.vectorValues;
		cellContents = snapshot.cellContents;
		probabilities = snapshot.probabilities;
		groups = snapshot.groups;
		
		currentSnapshotSlot = slot;
		
		updateVectorSizes();
		generateWeightedOutputs();
	}
		
	void renderSnapshotInspector() {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));  // Blue color
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
			
			if(ImGui::Button("Clear All Snapshots")) {
				ImGui::OpenPopup("Clear All?");
			}
			
			ImGui::PopStyleColor(3);
			
			if(ImGui::BeginPopupModal("Clear All?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Delete all snapshots?");
				ImGui::Separator();
				
				if(ImGui::Button("Yes")) {
					snapshots.clear();
					currentSnapshotSlot = -1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if(ImGui::Button("No")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			
			for(auto it = snapshots.begin(); it != snapshots.end();) {
				ImGui::PushID(it->first);
				
				bool shouldDelete = false;
				char nameBuf[256];
				strcpy(nameBuf, it->second.name.c_str());
				
				ImGui::Text("Slot %d", it->first);
				
				ImGui::SetNextItemWidth(150);
				if(ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
					it->second.name = nameBuf;
				}
				
				ImGui::SameLine();
				if(ImGui::Button("Load")) {
					loadSnapshot(it->first);
				}
				
				ImGui::SameLine();
				if(ImGui::Button("Update")) {
					storeSnapshot(it->first);
				}
				
				ImGui::SameLine();
				if(ImGui::Button("X")) {
					shouldDelete = true;
				}
				
				if(shouldDelete) {
					if(currentSnapshotSlot == it->first) {
						currentSnapshotSlot = -1;
					}
					it = snapshots.erase(it);
				} else {
					++it;
				}
				
				ImGui::PopID();
				ImGui::Separator();
			}
		}
	   
	   void saveSnapshotToJson(const DropdownListSnapshot& snapshot, ofJson& json) {
		   json["name"] = snapshot.name;
		   json["bars"] = snapshot.bars;
		   json["notegrid"] = snapshot.notegrid;
		   json["bpm"] = snapshot.bpm;
		   json["everyN"] = snapshot.everyN;
		   json["root"] = snapshot.root;
		   json["palette"] = snapshot.palette;
		   json["scale"] = snapshot.scale;
		   json["transposeWeights"] = snapshot.transposeWeights;
		   json["degreeWeights"] = snapshot.degreeWeights;
		   
		   ofJson cellContentsJson;
		   for(const auto& cell : snapshot.cellContents) {
			   cellContentsJson.push_back({
				   {"type", cell.first},
				   {"value", cell.second}
			   });
		   }
		   json["cellContents"] = cellContentsJson;
		   
		   json["probabilities"] = snapshot.probabilities;
		   json["groups"] = snapshot.groups;
	   }
	   
	   void loadSnapshotFromJson(DropdownListSnapshot& snapshot, const ofJson& json) {
		   snapshot.name = json["name"];
		   snapshot.bars = json["bars"];
		   snapshot.notegrid = json["notegrid"];
		   snapshot.bpm = json["bpm"];
		   snapshot.everyN = json["everyN"];
		   snapshot.root = json["root"];
		   snapshot.palette = json["palette"];
		   snapshot.scale = json["scale"];
		   snapshot.transposeWeights = json["transposeWeights"].get<vector<float>>();
		   snapshot.degreeWeights = json["degreeWeights"].get<vector<float>>();
		   
		   snapshot.cellContents.clear();
		   for(const auto& cell : json["cellContents"]) {
			   snapshot.cellContents.push_back({
				   cell["type"],
				   cell["value"]
			   });
		   }
		   
		   snapshot.probabilities = json["probabilities"].get<vector<int>>();
		   snapshot.groups = json["groups"].get<vector<int>>();
	   }
	
	void drawGui() {
			if(!ImGui::GetCurrentContext()) return;
			
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));
			
			float currentWidth = widgetWidth.get();
			float sliderHeight = 33;
			
			// Draw weight sliders with safety checks
			if(!transposeWeights.get().empty()) {
				drawWeightSliders("Transpose Weights", transposeWeights, sliderHeight, true);
			}
			
			if(!degreeWeights.get().empty()) {
				drawWeightSliders("Degree Weights", degreeWeights, sliderHeight, true);
			}
			
			ImGui::SetNextItemWidth(currentWidth);
			
			// Draw table with safety checks
			drawProbabilityTable(currentWidth);
			
			ImGui::PopStyleVar();
		}
	
	
	void drawProbabilityTable(float width) {
			if(ImGui::BeginTable("##ProbabilityTable", 6, ImGuiTableFlags_Borders, ImVec2(width, 0))) {
				ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 35.0f);
				ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, 35.0f);
				ImGui::TableSetupColumn("O", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("^", ImGuiTableColumnFlags_WidthFixed, 32.0f);
				ImGui::TableHeadersRow();
				
				int safeNumRows = std::min(static_cast<int>(numRows.get()),
										 static_cast<int>(cellContents.size()));
				
				for(int row = 0; row < safeNumRows; row++) {
					drawTableRow(row);
				}
				
				ImGui::EndTable();
			}
		}
	
	void drawTableRow(int row) {
			if(row >= cellContents.size() || row >= probabilities.size() ||
			   row >= groups.size() || row >= lastResults.size()) {
				return;
			}
			
			ImGui::TableNextRow();
			
			// Element column
			ImGui::TableSetColumnIndex(0);
			string displayText = cellContents[row].second.empty() ? "none" : cellContents[row].second;
			ImGui::PushID(row);
			if(ImGui::Selectable(displayText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
				ImGui::OpenPopup("dropdown_menu");
			}
			
			if(ImGui::BeginPopup("dropdown_menu")) {
				// Draw dropdown menus directly here instead of separate method
				if (ImGui::BeginMenu("Rhythm")) {
					for (const auto& rhythm : rhythmOptions) {
						if (ImGui::MenuItem(rhythm.c_str())) {
							cellContents[row] = std::make_pair("rhythm", rhythm);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Texture")) {
					for (const auto& texture : textureOptions) {
						if (ImGui::MenuItem(texture.c_str())) {
							cellContents[row] = std::make_pair("texture", texture);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Harmony")) {
					for (const auto& harmony : harmonyOptions) {
						if (ImGui::MenuItem(harmony.c_str())) {
							cellContents[row] = std::make_pair("harmony", harmony);
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("FX")) {
					for (const auto& fx : fxOptions) {
						if (ImGui::MenuItem(fx.c_str())) {
							cellContents[row] = std::make_pair("fx", fx);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();

			// Probability column
			ImGui::TableSetColumnIndex(1);
			int prob = probabilities[row];
			if (ImGui::DragInt(("##prob" + ofToString(row)).c_str(), &prob, 1, 0, 100)) {
				probabilities[row] = prob;
			}

			// Group column
			ImGui::TableSetColumnIndex(2);
			int group = groups[row];
			if (ImGui::DragInt(("##group" + ofToString(row)).c_str(), &group, 1, 0, 100)) {
				groups[row] = group;
			}

			// Result column
			ImGui::TableSetColumnIndex(3);
			ImGui::PushStyleColor(ImGuiCol_Button, lastResults[row] ?
				ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::Button(("##result" + ofToString(row)).c_str(), ImVec2(16, 16));
			ImGui::PopStyleColor();

			// Clear button
			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button(("X##" + ofToString(row)).c_str())) {
				cellContents[row] = std::make_pair("", "");
				probabilities[row] = 0;
				groups[row] = 0;
			}

			// Move buttons
			ImGui::TableSetColumnIndex(5);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			float arrowSize = 10.0f;
			ImGui::BeginGroup();
			if (row > 0) {
				if (ImGui::ArrowButton(("##up" + ofToString(row)).c_str(), ImGuiDir_Up)) {
					moveCell(row, row - 1);
				}
			} else {
				ImGui::InvisibleButton(("##up" + ofToString(row)).c_str(), ImVec2(arrowSize, arrowSize));
			}
			ImGui::SameLine(0, 2.0f);
			if (row < numRows - 1) {
				if (ImGui::ArrowButton(("##down" + ofToString(row)).c_str(), ImGuiDir_Down)) {
					moveCell(row, row + 1);
				}
			} else {
				ImGui::InvisibleButton(("##down" + ofToString(row)).c_str(), ImVec2(arrowSize, arrowSize));
			}
			ImGui::EndGroup();
			ImGui::PopStyleVar();
		}
	
	void drawWeightSliders(const string& label, ofParameter<vector<float>>& weights, float height, bool showLabels) {
			ImGui::Text("%s", label.c_str());
			
			float width = widgetWidth.get();
			ImGui::PushID(label.c_str());
			
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			
			// Background and frame
			drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
				ImGui::GetColorU32(ImGuiCol_FrameBg));
				
			float sliderWidth = width / 12.0f;
			
			// Create a mutable copy of the weights
			vector<float> weightsCopy = weights.get();
			
			// Invisible button for drag interaction
			ImGui::InvisibleButton("##dragArea", ImVec2(width, height));
			bool isDragging = ImGui::IsItemActive();
			
			if(isDragging) {
				ImVec2 mousePos = ImGui::GetIO().MousePos;
				
				// Calculate which slider we're over and update it
				int index = (mousePos.x - pos.x) / sliderWidth;
				if(index >= 0 && index < 12) {
					float normalizedY = 1.0f - (mousePos.y - pos.y) / height;
					weightsCopy[index] = ofClamp(normalizedY, 0.0f, 1.0f);
					weights.set(weightsCopy);
				}
			}
			
			// Draw sliders
			for(int i = 0; i < 12 && i < weightsCopy.size(); i++) {
				// Draw value
				float normalizedValue = weightsCopy[i];
				ImVec2 valuePosStart(pos.x + i * sliderWidth, pos.y + height * (1.0f - normalizedValue));
				ImVec2 valuePosEnd(pos.x + (i + 1) * sliderWidth - 2, pos.y + height);
				
				drawList->AddRectFilled(valuePosStart, valuePosEnd,
					ImGui::GetColorU32(ImGuiCol_PlotHistogram));
				
				// Draw label
				if(showLabels) {
					string label = ofToString(i);
					float textWidth = ImGui::CalcTextSize(label.c_str()).x;
					ImVec2 textPos(pos.x + i * sliderWidth + (sliderWidth - textWidth) * 0.5f,
						pos.y + height + 2);
					drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
				}
			}
			
			ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + (showLabels ? 20 : 4)));
			ImGui::PopID();
		}
	
	void generateWeightedOutputs() {
			// Safety check for vector sizes
			if(transposeWeights.get().size() != 12 || degreeWeights.get().size() != 12) {
				ofLogWarning("probabilityDropdownList") << "Invalid weights vector size";
				return;
			}
			
			// Update vector sizes safely
			updateVectorSizes();
			
			vector<int> newTranspose(bars);
			vector<int> newDegrees(bars);
			
			// Generate weighted random values with bounds checking
			for(int i = 0; i < bars && i < newTranspose.size() && i < newDegrees.size(); i++) {
				newTranspose[i] = getWeightedRandomIndex(transposeWeights);
				newDegrees[i] = getWeightedRandomIndex(degreeWeights);
			}
			
			// Only set output if vectors are valid
			if(newTranspose.size() == bars && newDegrees.size() == bars) {
				transposeOut = newTranspose;
				degreeOut = newDegrees;
			}
		}
	   
	int getWeightedRandomIndex(const ofParameter<vector<float>>& weights) {
			const auto& weightVec = weights.get();
			if(weightVec.empty()) return 0;
			
			float sum = 0;
			for(float w : weightVec) {
				if(w >= 0) {  // Only add non-negative weights
					sum += w;
				}
			}
			
			if(sum <= 0) return 0;
			
			float r = ofRandom(sum);
			float cumsum = 0;
			
			for(size_t i = 0; i < weightVec.size(); i++) {
				if(weightVec[i] >= 0) {  // Only consider non-negative weights
					cumsum += weightVec[i];
					if(r <= cumsum) return i;
				}
			}
			
			return weightVec.size() - 1;
		}
	   
	void updateVectorSizes() {
			int currentBars = bars.get();
			if(currentBars < 1) currentBars = 1;  // Safety check
			
			vector<int> newTranspose(currentBars, 0);
			vector<int> newDegrees(currentBars, 0);
			
			// Safely copy existing values
			const auto& currentTranspose = transposeOut.get();
			const auto& currentDegrees = degreeOut.get();
			
			for(int i = 0; i < currentBars; i++) {
				if(i < static_cast<int>(currentTranspose.size())) {
					newTranspose[i] = currentTranspose[i];
				}
				if(i < static_cast<int>(currentDegrees.size())) {
					newDegrees[i] = currentDegrees[i];
				}
			}
			
			transposeOut = newTranspose;
			degreeOut = newDegrees;
		}
	
	void moveCell(int fromIdx, int toIdx) {
			if(fromIdx < 0 || fromIdx >= numRows ||
			   toIdx < 0 || toIdx >= numRows ||
			   fromIdx >= cellContents.size() ||
			   toIdx >= cellContents.size() ||
			   fromIdx >= probabilities.size() ||
			   toIdx >= probabilities.size() ||
			   fromIdx >= groups.size() ||
			   toIdx >= groups.size() ||
			   fromIdx >= lastResults.size() ||
			   toIdx >= lastResults.size()) {
				return;
			}
			
			std::swap(cellContents[fromIdx], cellContents[toIdx]);
			std::swap(probabilities[fromIdx], probabilities[toIdx]);
			std::swap(groups[fromIdx], groups[toIdx]);
			std::swap(lastResults[fromIdx], lastResults[toIdx]);
		}
	
	void generateOutput() {
		string result;
		
		// Check global probability
		float globalRandom = ofRandom(100);
		bool globalPass = (globalRandom < globalProb);
		
		// If global probability fails, clear everything and return
		if (!globalPass) {
			output = "";
			for(int i = 0; i < numRows; i++) {
				lastResults[i] = false;
			}
			return;
		}

		// First determine which groups are active this round
		vector<bool> activeGroups(groupProb->size(), false);
		for(int i = 0; i < groupProb->size(); i++) {
			float random = ofRandom(1.0f);
			activeGroups[i] = (random < groupProb.get()[i]);
		}
		
		// Process each cell
		for(int i = 0; i < numRows; i++) {
			// Skip empty cells or cells with "none"
			if(cellContents[i].second.empty() || cellContents[i].second == "none") {
				lastResults[i] = false;
				continue;
			}
			
			// Check if element's group is active
			int elementGroup = groups[i];
			bool groupActive = (elementGroup >= groupProb->size()) || activeGroups[elementGroup];
			
			if(groupActive) {
				// Only check individual probability if group is active
				float random = ofRandom(100);
				bool succeeded = (random < probabilities[i]);
				lastResults[i] = succeeded;
				
				if(succeeded) {
					if(!result.empty()) result += " ";
					result += cellContents[i].second;
				}
			} else {
				lastResults[i] = false;
			}
		}
		
		output = result;
	}

private:
	ofEventListeners listeners;
		ofParameter<int> numSliders;
		vector<string> paletteOptions;
		vector<string> scaleOptions;
		vector<string> rhythmOptions;
		vector<string> textureOptions;
		vector<string> harmonyOptions;
		vector<string> fxOptions;
		vector<pair<string, string>> cellContents;
		vector<int> probabilities;
		vector<int> groups;
		vector<bool> lastResults;
		vector<vector<vector<float>>> vectorValues;
		vector<ofParameter<vector<float>>> vectorValueParams;
		vector<int> currentToEditValues;
		
		// Parameters
		ofParameter<int> bars;
		ofParameter<int> notegrid;
		ofParameter<int> bpm;
		ofParameter<int> everyN;
		ofParameter<int> root;
		ofParameter<int> palette;
		ofParameter<int> scale;
		ofParameter<vector<float>> transposeWeights;
		ofParameter<vector<float>> degreeWeights;
		ofParameter<vector<int>> transposeOut;
		ofParameter<vector<int>> degreeOut;
		ofParameter<void> trigger;
		ofParameter<string> output;
		ofParameter<string> elementsPath;
		ofParameter<void> browseButton;
		ofParameter<int> widgetWidth;
		ofParameter<int> numRows;
		ofParameter<int> globalProb;
		ofParameter<vector<float>> groupProb;
		customGuiRegion guiRegion;
		
		// Snapshot system members
		std::map<int, DropdownListSnapshot> snapshots;
		int currentSnapshotSlot;
		ofParameter<int> activeSnapshotSlot;
		ofParameter<bool> showSnapshotNames;
		ofParameter<void> addSnapshotButton;
		ofParameter<std::function<void()>> snapshotInspector;
		ofEventListeners snapshotListeners;
	
	void loadElementsFromFile(const string& path) {
		if(path.empty()) return;
		
		// Reset lists but keep "none" option
		rhythmOptions = {"none"};
		textureOptions = {"none"};
		harmonyOptions = {"none"};
		fxOptions = {"none"};
		
		// Read file
		ofFile file(path);
		if(!file.exists()) {
			ofLogError("probabilityDropdownList") << "File not found: " << path;
			return;
		}

		// Read file line by line
		ofBuffer buffer = ofBufferFromFile(path);
		for(auto line : buffer.getLines()) {
			string element = ofTrim(line);
			if(element.empty()) continue;
			
			// Categorize based on prefix
			if(element.substr(0,2) == "fx") {
				fxOptions.push_back(element);
			}
			else if(element.substr(0,1) == "r") {
				rhythmOptions.push_back(element);
			}
			else if(element.substr(0,1) == "h") {
				harmonyOptions.push_back(element);
			}
			else if(element.substr(0,1) == "t") {
				textureOptions.push_back(element);
			}
		}
	}
};

#endif /* probabilityDropdownList_h */
