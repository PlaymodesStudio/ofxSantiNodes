#ifndef schoenbergMatrix_h
#define schoenbergMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <set>
#include <algorithm>
#include <random>

class schoenbergMatrix : public ofxOceanodeNodeModel {
public:
	schoenbergMatrix() : ofxOceanodeNodeModel("Schoenberg Matrix"), rng(std::random_device{}()) {}
	
	void setup() override {
		description = "Twelve-tone serial matrix generator with experimental features. "
					  "Standard dodecaphonic technique with optional chaos/generative controls.";
		
		initializePresets();
		
		// ── TONE ROW ──
		addSeparator("Tone Row", ofColor(200));
		addParameterDropdown(presetSelect, "Preset", 0,
			{"User", "Schoenberg Op.25", "Berg Violin", "Webern Sym.21", "Boulez Structures"});
		addParameter(primeRow.set("Row", {0,1,2,3,4,5,6,7,8,9,10,11}, {0}, {11}));
		addParameter(validate.set("Validate", true));
		addParameter(randomRow.set("Random Row"));
		addParameter(mutationAmount.set("Mutation", 0, 0, 12));
		
		// ── MATRIX NAVIGATION ──
		addSeparator("Navigation", ofColor(200));
		addParameterDropdown(formSelect, "Form", 0, {"Prime", "Retrograde", "Inversion", "Retro-Inv"});
		addParameter(transposition.set("Transpose", 0, 0, 11));
		addParameter(matrixRow.set("Matrix Row", 0, 0, 47));
		
		// ── EXTRACTION ──
		addSeparator("Extraction", ofColor(200));
		addParameter(fullRowMode.set("Full Row", true));
		addParameter(segmentStart.set("Start Pos", 0, 0, 11));
		addParameter(segmentLength.set("Length", 12, 1, 12));
		addParameter(autoAdvance.set("Auto Advance", false));
		addParameter(stride.set("Stride", 1, 1, 12));
		
		// ── TRANSFORMATIONS ──
		addSeparator("Transform", ofColor(200));
		addParameter(octaveTranspose.set("Octave", 0, -4, 4));
		addParameter(octaveSpread.set("Oct Spread", 1, 1, 4));
		addParameter(rotation.set("Rotation", 0, 0, 11));
		
		// ── EXPERIMENTAL ──
		addSeparator("Experimental", ofColor(200));
		addParameter(chaosAmount.set("Chaos", 0.0f, 0.0f, 1.0f));
		addParameter(probabilityMask.set("Prob Mask",
			{1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f},
			{0.0f}, {1.0f}));
		
		// ── RANDOMIZATION ──
		addSeparator("Randomize", ofColor(200));
		addParameter(randFormOnCalc.set("Rand Form", false));
		addParameter(randTransOnCalc.set("Rand Trans", false));
		addParameter(randSegmentOnCalc.set("Rand Segment", false));
		
		// ── DISPLAY ──
		addSeparator("Display", ofColor(200));
		addCustomRegion(guiRegion.set("Matrix Display", [this](){
			drawMatrixDisplay();
		}), [this](){
			drawMatrixDisplay();
		});
		
		// ── OUTPUT ──
		addSeparator("Output", ofColor(200));
		addOutputParameter(pitchOut.set("Pitch[]", {0}, {0}, {127}));
		addOutputParameter(fullRowOut.set("Full Row[]", {0}, {0}, {127}));
		addOutputParameter(complementOut.set("Complement[]", {0}, {0}, {127}));
		addOutputParameter(isValid.set("Is Valid", false));
		addOutputParameter(currentMatrixRow.set("Current Row", 0, 0, 47));
		
		// Listeners
		listeners.push(presetSelect.newListener([this](int &v){ loadPreset(v); }));
		listeners.push(primeRow.newListener([this](vector<int> &){ calculate(); }));
		listeners.push(validate.newListener([this](bool &){ calculate(); }));
		listeners.push(randomRow.newListener([this](){ generateRandomRow(); }));
		listeners.push(mutationAmount.newListener([this](int &){
			if(mutationAmount > 0) mutateRow();
		}));
		
		listeners.push(formSelect.newListener([this](int &){
			updateMatrixRowFromForm();
			calculate();
		}));
		listeners.push(transposition.newListener([this](int &){
			updateMatrixRowFromForm();
			calculate();
		}));
		listeners.push(matrixRow.newListener([this](int &){
			updateFormFromMatrixRow();
			calculate();
		}));
		
		listeners.push(fullRowMode.newListener([this](bool &){ calculate(); }));
		listeners.push(segmentStart.newListener([this](int &){ calculate(); }));
		listeners.push(segmentLength.newListener([this](int &){ calculate(); }));
		listeners.push(autoAdvance.newListener([this](bool &){ calculate(); }));
		listeners.push(stride.newListener([this](int &){ calculate(); }));
		
		listeners.push(octaveTranspose.newListener([this](int &){ calculate(); }));
		listeners.push(octaveSpread.newListener([this](int &){ calculate(); }));
		listeners.push(rotation.newListener([this](int &){ calculate(); }));
		
		listeners.push(chaosAmount.newListener([this](float &){ calculate(); }));
		listeners.push(probabilityMask.newListener([this](vector<float> &){ calculate(); }));
		
		calculate();
	}
	
private:
	// Tone Row
	ofParameter<int> presetSelect;
	ofParameter<vector<int>> primeRow;
	ofParameter<bool> validate;
	ofParameter<void> randomRow;
	ofParameter<int> mutationAmount;
	
	// Navigation
	ofParameter<int> formSelect;
	ofParameter<int> transposition;
	ofParameter<int> matrixRow;
	
	// Extraction
	ofParameter<bool> fullRowMode;
	ofParameter<int> segmentStart;
	ofParameter<int> segmentLength;
	ofParameter<bool> autoAdvance;
	ofParameter<int> stride;
	
	// Transformations
	ofParameter<int> octaveTranspose;
	ofParameter<int> octaveSpread;
	ofParameter<int> rotation;
	
	// Experimental
	ofParameter<float> chaosAmount;
	ofParameter<vector<float>> probabilityMask;
	
	// Randomization
	ofParameter<bool> randFormOnCalc;
	ofParameter<bool> randTransOnCalc;
	ofParameter<bool> randSegmentOnCalc;
	
	// Outputs
	ofParameter<vector<int>> pitchOut;
	ofParameter<vector<int>> fullRowOut;
	ofParameter<vector<int>> complementOut;
	ofParameter<bool> isValid;
	ofParameter<int> currentMatrixRow;
	
	ofEventListeners listeners;
	customGuiRegion guiRegion;
	std::mt19937 rng;
	
	std::map<int, std::vector<int>> presetRows;
	std::vector<std::vector<int>> matrix12x12;  // Traditional 12×12 matrix
	std::vector<std::vector<int>> matrix;  // 48 rows: 12 P, 12 R, 12 I, 12 RI
	int currentSegmentPos = 0;
	
	const char* noteNames[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
	
	void initializePresets() {
		presetRows[1] = {4,5,7,1,6,3,8,2,11,0,9,10};  // Schoenberg Suite Op. 25
		presetRows[2] = {0,3,7,9,11,1,4,2,6,8,10,5};  // Berg Violin Concerto
		presetRows[3] = {11,10,2,3,7,6,8,4,5,0,1,9};  // Webern Symphony Op. 21
		presetRows[4] = {3,2,9,8,7,6,4,1,0,10,5,11};  // Boulez Structures
	}
	
	void loadPreset(int index) {
		if(index == 0) return;  // User mode
		if(presetRows.find(index) != presetRows.end()) {
			primeRow.setWithoutEventNotifications(presetRows[index]);
			calculate();
		}
	}
	
	void generateRandomRow() {
		std::vector<int> row = {0,1,2,3,4,5,6,7,8,9,10,11};
		std::shuffle(row.begin(), row.end(), rng);
		primeRow = row;
	}
	
	void mutateRow() {
		if(mutationAmount.get() == 0) return;
		
		std::vector<int> row = primeRow.get();
		if(row.size() < 2) return;
		
		int swaps = std::min((int)mutationAmount.get(), (int)row.size() / 2);
		for(int i = 0; i < swaps; i++) {
			std::uniform_int_distribution<int> dist(0, row.size() - 1);
			int pos1 = dist(rng);
			int pos2 = dist(rng);
			std::swap(row[pos1], row[pos2]);
		}
		
		primeRow = row;
		mutationAmount.setWithoutEventNotifications(0);
	}
	
	bool validateRow(const std::vector<int>& row) {
		if(row.size() != 12) return false;
		std::set<int> uniqueNotes;
		for(int note : row) {
			if(note < 0 || note > 11) return false;
			uniqueNotes.insert(note);
		}
		return uniqueNotes.size() == 12;
	}
	
	std::vector<int> getPrimeForm(const std::vector<int>& baseRow, int trans) {
		std::vector<int> result;
		for(int note : baseRow) {
			result.push_back((note + trans) % 12);
		}
		return result;
	}
	
	std::vector<int> getRetrogradeForm(const std::vector<int>& baseRow, int trans) {
		std::vector<int> result = getPrimeForm(baseRow, trans);
		std::reverse(result.begin(), result.end());
		return result;
	}
	
	std::vector<int> getInversionForm(const std::vector<int>& baseRow, int trans) {
		std::vector<int> result;
		int firstNote = baseRow[0];
		for(int note : baseRow) {
			int interval = (note - firstNote + 12) % 12;
			int inverted = (trans - interval + 12) % 12;
			result.push_back(inverted);
		}
		return result;
	}
	
	std::vector<int> getRetroInversionForm(const std::vector<int>& baseRow, int trans) {
		std::vector<int> result = getInversionForm(baseRow, trans);
		std::reverse(result.begin(), result.end());
		return result;
	}
	
	void generate12x12Matrix() {
		matrix12x12.clear();
		std::vector<int> row = primeRow.get();
		
		// Ensure we have 12 notes
		while(row.size() < 12) row.push_back(0);
		if(row.size() > 12) row.resize(12);
		
		// Traditional 12×12 matrix construction
		// First row is P0 (the prime row)
		// First column is I0 (inversion starting on same first note)
		
		std::vector<int> I0 = getInversionForm(row, row[0]);
		
		// Generate all 12 rows (P0 through P11)
		for(int r = 0; r < 12; r++) {
			std::vector<int> matrixRow;
			int transposition = I0[r];
			for(int c = 0; c < 12; c++) {
				int note = (row[c] + transposition) % 12;
				matrixRow.push_back(note);
			}
			matrix12x12.push_back(matrixRow);
		}
	}
	
	void generateMatrix() {
		matrix.clear();
		std::vector<int> row = primeRow.get();
		
		// Ensure we have 12 notes (pad or truncate if needed)
		while(row.size() < 12) row.push_back(0);
		if(row.size() > 12) row.resize(12);
		
		// Generate all 48 rows: P0-P11, R0-R11, I0-I11, RI0-RI11
		for(int t = 0; t < 12; t++) {
			matrix.push_back(getPrimeForm(row, t));
		}
		for(int t = 0; t < 12; t++) {
			matrix.push_back(getRetrogradeForm(row, t));
		}
		for(int t = 0; t < 12; t++) {
			matrix.push_back(getInversionForm(row, t));
		}
		for(int t = 0; t < 12; t++) {
			matrix.push_back(getRetroInversionForm(row, t));
		}
	}
	
	void updateMatrixRowFromForm() {
		int form = formSelect.get();
		int trans = transposition.get();
		int row = form * 12 + trans;
		matrixRow.setWithoutEventNotifications(row);
	}
	
	void updateFormFromMatrixRow() {
		int row = matrixRow.get();
		int form = row / 12;
		int trans = row % 12;
		formSelect.setWithoutEventNotifications(form);
		transposition.setWithoutEventNotifications(trans);
	}
	
	std::vector<int> applyRotation(const std::vector<int>& row) {
		if(rotation.get() == 0) return row;
		std::vector<int> result = row;
		int rot = rotation.get() % row.size();
		std::rotate(result.begin(), result.begin() + rot, result.end());
		return result;
	}
	
	std::vector<int> applyChaos(const std::vector<int>& row) {
		if(chaosAmount.get() <= 0.0f) return row;
		
		std::vector<int> result = row;
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		
		for(size_t i = 0; i < result.size() - 1; i++) {
			if(dist(rng) < chaosAmount.get()) {
				std::swap(result[i], result[i + 1]);
			}
		}
		
		return result;
	}
	
	std::vector<int> applyProbabilityMask(const std::vector<int>& row) {
		std::vector<float> mask = probabilityMask.get();
		while(mask.size() < 12) mask.push_back(1.0f);
		
		std::vector<int> result;
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		
		for(size_t i = 0; i < row.size() && i < mask.size(); i++) {
			if(dist(rng) < mask[i]) {
				result.push_back(row[i]);
			}
		}
		
		return result;
	}
	
	std::vector<int> getComplement(const std::vector<int>& used) {
		std::set<int> usedSet;
		for(int note : used) {
			usedSet.insert(note % 12);
		}
		
		std::vector<int> complement;
		for(int i = 0; i < 12; i++) {
			if(usedSet.find(i) == usedSet.end()) {
				complement.push_back(i);
			}
		}
		return complement;
	}
	
	void calculate() {
		// Apply randomization
		if(randFormOnCalc.get()) {
			std::uniform_int_distribution<int> dist(0, 3);
			formSelect.setWithoutEventNotifications(dist(rng));
		}
		if(randTransOnCalc.get()) {
			std::uniform_int_distribution<int> dist(0, 11);
			transposition.setWithoutEventNotifications(dist(rng));
		}
		if(randSegmentOnCalc.get()) {
			std::uniform_int_distribution<int> distStart(0, 11);
			std::uniform_int_distribution<int> distLen(3, 6);
			segmentStart.setWithoutEventNotifications(distStart(rng));
			segmentLength.setWithoutEventNotifications(distLen(rng));
		}
		
		// Validate row
		std::vector<int> row = primeRow.get();
		bool valid = validateRow(row);
		isValid = valid;
		
		if(validate.get() && !valid) {
			// If validation is on and row is invalid, output empty
			pitchOut = std::vector<int>();
			fullRowOut = std::vector<int>();
			complementOut = std::vector<int>();
			return;
		}
		
		// Generate matrices
		generate12x12Matrix();
		generateMatrix();
		
		// Get current row from matrix
		int rowIndex = matrixRow.get();
		if(rowIndex < 0 || rowIndex >= (int)matrix.size()) rowIndex = 0;
		std::vector<int> currentRow = matrix[rowIndex];
		currentMatrixRow = rowIndex;
		
		// Apply rotation
		currentRow = applyRotation(currentRow);
		
		// Apply chaos
		currentRow = applyChaos(currentRow);
		
		// Extract segment or full row
		std::vector<int> segment;
		if(fullRowMode.get()) {
			segment = currentRow;
		} else {
			// Auto-advance position
			if(autoAdvance.get()) {
				currentSegmentPos = (currentSegmentPos + stride.get()) % 12;
				segmentStart.setWithoutEventNotifications(currentSegmentPos);
			}
			
			int start = segmentStart.get();
			int length = segmentLength.get();
			
			for(int i = 0; i < length; i++) {
				int index = (start + i) % currentRow.size();
				segment.push_back(currentRow[index]);
			}
		}
		
		// Apply probability mask
		segment = applyProbabilityMask(segment);
		
		// Apply octave spread and transpose
		std::vector<int> finalPitches;
		for(size_t i = 0; i < segment.size(); i++) {
			int octaveOffset = (i % octaveSpread.get()) * 12;
			int pitch = segment[i] + (octaveTranspose.get() * 12) + octaveOffset;
			finalPitches.push_back(pitch);
		}
		
		// Calculate complement
		std::vector<int> complement = getComplement(segment);
		
		// Output
		pitchOut = finalPitches;
		fullRowOut = currentRow;
		complementOut = complement;
	}
	
	void drawMatrixDisplay() {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		if(matrix12x12.empty()) return;
		
		float cellSize = 18;
		float labelWidth = 20;
		float matrixSize = cellSize * 12;
		float totalWidth = matrixSize + labelWidth + 10;
		float totalHeight = matrixSize + labelWidth + 60;
		
		ImGui::InvisibleButton("MatrixDisplay", ImVec2(totalWidth, totalHeight));
		
		ImU32 bgColor = IM_COL32(25, 25, 25, 255);
		ImU32 cellColor = IM_COL32(50, 50, 50, 255);
		ImU32 primeHighlight = IM_COL32(100, 150, 100, 255);
		ImU32 inversionHighlight = IM_COL32(100, 100, 150, 255);
		ImU32 textColor = IM_COL32(200, 200, 200, 255);
		ImU32 labelColor = IM_COL32(150, 150, 150, 255);
		
		// Draw validation status
		bool valid = isValid.get();
		ImU32 validColor = valid ? IM_COL32(80, 200, 80, 255) : IM_COL32(200, 80, 80, 255);
		const char* validText = valid ? "VALID" : "INVALID";
		
		drawList->AddRectFilled(
			ImVec2(pos.x, pos.y),
			ImVec2(pos.x + 60, pos.y + 18),
			validColor, 3.0f);
		drawList->AddText(ImVec2(pos.x + 8, pos.y + 2), IM_COL32(255,255,255,255), validText);
		
		// Draw matrix title
		drawList->AddText(ImVec2(pos.x + 70, pos.y + 2), textColor, "12×12 MATRIX (Rows=P, Cols=I)");
		
		float matrixStartY = pos.y + 25;
		float matrixStartX = pos.x + labelWidth;
		
		// Draw column labels (I0 - I11) at top
		for(int c = 0; c < 12; c++) {
			char label[8];
			snprintf(label, sizeof(label), "I%d", c);
			float x = matrixStartX + c * cellSize;
			drawList->AddText(ImVec2(x + 2, matrixStartY - 15), labelColor, label);
		}
		
		// Determine which row/column is selected based on current form
		int highlightRow = -1;
		int highlightCol = -1;
		
		int form = formSelect.get();
		int trans = transposition.get();
		
		if(form == 0) {  // Prime - highlight row
			highlightRow = trans;
		} else if(form == 2) {  // Inversion - highlight column
			highlightCol = trans;
		}
		
		// Draw 12×12 matrix
		for(int r = 0; r < 12; r++) {
			// Row label (P0 - P11)
			char rowLabel[8];
			snprintf(rowLabel, sizeof(rowLabel), "P%d", r);
			drawList->AddText(ImVec2(pos.x, matrixStartY + r * cellSize + 2), labelColor, rowLabel);
			
			for(int c = 0; c < 12; c++) {
				float x = matrixStartX + c * cellSize;
				float y = matrixStartY + r * cellSize;
				
				// Determine cell color
				ImU32 color = cellColor;
				if(r == highlightRow) color = primeHighlight;
				if(c == highlightCol) color = inversionHighlight;
				
				drawList->AddRectFilled(
					ImVec2(x, y),
					ImVec2(x + cellSize - 1, y + cellSize - 1),
					color, 1.0f);
				
				// Draw note number
				if(r < (int)matrix12x12.size() && c < (int)matrix12x12[r].size()) {
					char noteText[4];
					snprintf(noteText, sizeof(noteText), "%d", matrix12x12[r][c]);
					ImVec2 textSize = ImGui::CalcTextSize(noteText);
					drawList->AddText(
						ImVec2(x + (cellSize - textSize.x) / 2, y + 2),
						textColor, noteText);
				}
			}
		}
		
		// Info line
		float infoY = matrixStartY + matrixSize + 8;
		const char* formNames[4] = {"Prime", "Retrograde", "Inversion", "Retro-Inv"};
		
		char info[128];
		snprintf(info, sizeof(info), "%s-%d | Row %d | Segment: %d-%d | Out: %zu notes",
			formNames[formSelect.get()], transposition.get(),
			matrixRow.get(), segmentStart.get(),
			(segmentStart.get() + segmentLength.get() - 1) % 12,
			pitchOut.get().size());
		
		drawList->AddText(ImVec2(pos.x, infoY), textColor, info);
		
		// Draw current row linearly below
		float rowViewY = infoY + 20;
		drawList->AddText(ImVec2(pos.x, rowViewY), labelColor, "Current Row:");
		
		std::vector<int> currentRow = fullRowOut.get();
		for(size_t i = 0; i < currentRow.size() && i < 12; i++) {
			float x = pos.x + i * 20;
			float y = rowViewY + 15;
			
			// Highlight segment
			bool inSegment = false;
			if(!fullRowMode.get()) {
				int start = segmentStart.get();
				int length = segmentLength.get();
				for(int j = 0; j < length; j++) {
					if((start + j) % 12 == (int)i) {
						inSegment = true;
						break;
					}
				}
			}
			
			ImU32 cellCol = inSegment ? IM_COL32(120, 180, 120, 255) : cellColor;
			
			drawList->AddRectFilled(
				ImVec2(x, y),
				ImVec2(x + 18, y + 18),
				cellCol, 2.0f);
			
			char noteText[4];
			snprintf(noteText, sizeof(noteText), "%d", currentRow[i]);
			ImVec2 textSize = ImGui::CalcTextSize(noteText);
			drawList->AddText(
				ImVec2(x + (18 - textSize.x) / 2, y + 2),
				textColor, noteText);
		}
	}
};

#endif /* schoenbergMatrix_h */
