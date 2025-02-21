#ifndef probabilityDropdownList_h
#define probabilityDropdownList_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"

class probabilityDropdownList : public ofxOceanodeNodeModel {
public:
	probabilityDropdownList() : ofxOceanodeNodeModel("Probability Dropdown List") {
		description = "Generates a space-separated list of strings based on probability values and group modifiers";
	}
	
	void setup() override {
		// Initialize with "none" option
		rhythmOptions = {"none"};
		textureOptions = {"none"};
		harmonyOptions = {"none"};
		fxOptions = {"none"};

		// Add path parameter
		addParameter(elementsPath.set("Path", ""));
		addParameter(browseButton.set("Open"));
		
		// Add file load listener
		listeners.push(elementsPath.newListener([this](string &path){
			loadElementsFromFile(path);
		}));

		// Add parameters
		addParameter(widgetWidth.set("Width", 360, 100, 500));
		addParameter(numRows.set("Rows", 8, 1, 32));
		addParameter(globalProb.set("Global %", 100, 0, 100));
		addParameter(groupProb.set("Group %", {1.0f}, {0.0f}, {1.0f}));
		addParameter(trigger.set("GO"));
		addOutputParameter(output.set("Output", ""));
		

		
		// Initialize data structures
		cellContents.resize(32);  // Maximum size
		probabilities.resize(32, 0);
		groups.resize(32, 0);
		lastResults.resize(32, false);
		
		// Add custom region for ImGui interface
		addCustomRegion(guiRegion, [this](){
			drawGui();
		});
		
		// Setup listeners
		listeners.push(trigger.newListener([this]() {
			generateOutput();
		}));
		
		listeners.push(browseButton.newListener([this]() {
			ofFileDialogResult result = ofSystemLoadDialog("Select elements file", false);
			if(result.bSuccess) {
				elementsPath = result.getPath();
			}
		}));

		// Setup row count listener
		listeners.push(numRows.newListener([this](int &n) {
			if(cellContents.size() > n) {
				cellContents.resize(n);
				probabilities.resize(n);
				groups.resize(n);
				lastResults.resize(n);
			}
		}));
	}

	void presetSave(ofJson &json) override {
		// Save cell contents, probabilities and groups
		string cellDataStr;
		string probDataStr;
		string groupDataStr;
		
		for(int i = 0; i < numRows; i++) {
			if(!cellContents[i].second.empty()) {
				cellDataStr += cellContents[i].first + ":" + cellContents[i].second;
				probDataStr += ofToString(probabilities[i]);
				groupDataStr += ofToString(groups[i]);
				
				if(i < numRows - 1) {
					cellDataStr += "|";
					probDataStr += "|";
					groupDataStr += "|";
				}
			}
		}
		
		json["cellData"] = cellDataStr;
		json["probData"] = probDataStr;
		json["groupData"] = groupDataStr;
		json["numRows"] = numRows.get();
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("cellData") && json.count("probData") && json.count("groupData") && json.count("numRows")) {
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
	}

private:
	void drawGui() {
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));
		
		float currentWidth = widgetWidth.get();
		ImGui::SetNextItemWidth(currentWidth);
		
		// Main probability table
		if (ImGui::BeginTable("##ProbabilityTable", 6, ImGuiTableFlags_Borders, ImVec2(currentWidth, 0))) {
			ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 35.0f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, 35.0f);
			ImGui::TableSetupColumn("O", ImGuiTableColumnFlags_WidthFixed, 16);
			ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 16);
			ImGui::TableSetupColumn("^", ImGuiTableColumnFlags_WidthFixed, 32.0f);
			ImGui::TableHeadersRow();

			for (int row = 0; row < numRows; row++) {
				ImGui::TableNextRow();
				
				// Element column with dropdown
				ImGui::TableSetColumnIndex(0);
				string displayText = cellContents[row].second.empty() ? "none" : cellContents[row].second;
				ImGui::PushID(row);
				if (ImGui::Selectable(displayText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
					ImGui::OpenPopup("dropdown_menu");
				}

				if (ImGui::BeginPopup("dropdown_menu")) {
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
			ImGui::EndTable();
		}
		
		ImGui::PopStyleVar();
	}
	
	void moveCell(int fromIdx, int toIdx) {
		if (fromIdx < 0 || fromIdx >= numRows || toIdx < 0 || toIdx >= numRows) return;
		
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
	vector<string> rhythmOptions;
	vector<string> textureOptions;
	vector<string> harmonyOptions;
	vector<string> fxOptions;
	
	vector<pair<string, string>> cellContents;  // (category, value)
	vector<int> probabilities;
	vector<int> groups;
	vector<bool> lastResults;
	
	ofParameter<void> trigger;
	ofParameter<string> output;
	ofParameter<int> widgetWidth;
	ofParameter<int> numRows;
	ofParameter<int> globalProb;
	ofParameter<vector<float>> groupProb;
	ofParameter<string> elementsPath;
	ofParameter<void> browseButton;
	
	customGuiRegion guiRegion;

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
	ofEventListeners listeners;
};

#endif /* probabilityDropdownList_h */
