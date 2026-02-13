#ifndef bartokAxis_h
#define bartokAxis_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <map>
#include <random>

class bartokAxis : public ofxOceanodeNodeModel {
public:
	bartokAxis() : ofxOceanodeNodeModel("Bartok Axis"), rng(std::random_device{}()) {}
	
	void setup() override {
		description = "Generates MIDI pitches based on Béla Bartók's axis system. "
					  "Three functional axes (T/S/D) with 4 poles each. "
					  "Configurable intervals and chromatic deviation.";
		
		initializeModes();
		
		// ── AXIS SYSTEM ──
		addSeparator("Axis", ofColor(200));
		addParameter(root.set("Root", 0, 0, 11));
		addParameter(axis.set("Axis", 0, 0, 2));
		addParameter(pole.set("Pole", 0, 0, 3));
		
		// ── INTERVALS ──
		addSeparator("Intervals", ofColor(200));
		addParameter(axisInterval.set("Axis Int", 7, 1, 11));
		addParameter(poleInterval.set("Pole Int", 3, 1, 11));
		
		// ── SCALE / MODE ──
		addSeparator("Scale", ofColor(200));
		addParameterDropdown(modeSelect, "Mode", 0,
			{"Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
			 "Harm Min", "Harm Maj", "Mel Min", "User"});
		addParameter(userScale.set("User Scale", {0, 2, 4, 5, 7, 9, 11}, {0}, {11}));
		
		// ── MODE RANDOMIZATION ──
		addSeparator("Mode Random", ofColor(200));
		addParameter(randModeOnAxis.set("Rand on Axis", false));
		addParameter(randModeOnPole.set("Rand on Pole", false));
		addParameter(majorProb.set("Major Prob", 0.5f, 0.0f, 1.0f));
		
		// ── CHORD ──
		addSeparator("Chord", ofColor(200));
		addParameter(degree.set("Degree", 1, 1, 7));
		addParameter(chordSize.set("Chord Size", 3, 1, 7));
		addParameter(chordInterval.set("Chord Int", 2, 1, 6));
		
		// ── RANDOMIZATION ──
		addSeparator("Randomize", ofColor(200));
		addParameter(randDegreeOnAxis.set("Deg on Axis", false));
		addParameter(randDegreeOnPole.set("Deg on Pole", false));
		addParameter(randPoleOnAxis.set("Pole on Axis", false));
		
		// ── CHROMATIC DEVIATION ──
		addSeparator("Deviation", ofColor(200));
		addParameter(deviationProb.set("Dev Prob", 0.0f, 0.0f, 1.0f));
		addParameter(deviationRange.set("Dev Range", 1, 1, 6));
		
		// ── DISPLAY ──
		addSeparator("Display", ofColor(200));
		addCustomRegion(guiRegion.set("Axis Display", [this](){
			drawAxisDisplay();
		}), [this](){
			drawAxisDisplay();
		});
		
		// ── OUTPUT ──
		addSeparator("Output", ofColor(200));
		addOutputParameter(pitchOut.set("Pitch[]", {0}, {0}, {127}));
		addOutputParameter(scaleOut.set("Scale[]", {0}, {0}, {127}));
		addOutputParameter(rootNoteOut.set("Root Note", 0, 0, 11));
		addOutputParameter(chordRootOut.set("Chord Root", 0, 0, 11));
		addOutputParameter(currentModeOut.set("Current Mode", 0, 0, 10));
		
		// Listeners
		listeners.push(root.newListener([this](int &){ calculate(); }));
		listeners.push(axisInterval.newListener([this](int &){ calculate(); }));
		listeners.push(poleInterval.newListener([this](int &){ calculate(); }));
		
		// Axis change listener - with randomization logic
		listeners.push(axis.newListener([this](int &){
			if(randModeOnAxis.get()) {
				randomizeMode();
			}
			if(randPoleOnAxis.get()) {
				std::uniform_int_distribution<int> dist(0, 3);
				pole.setWithoutEventNotifications(dist(rng));
			}
			if(randDegreeOnAxis.get()) {
				std::vector<int> intervals = getCurrentScaleIntervals();
				std::uniform_int_distribution<int> dist(1, intervals.size());
				degree.setWithoutEventNotifications(dist(rng));
			}
			calculate();
		}));
		
		// Pole change listener - with randomization logic
		listeners.push(pole.newListener([this](int &){
			if(randModeOnPole.get()) {
				randomizeMode();
			}
			if(randDegreeOnPole.get()) {
				std::vector<int> intervals = getCurrentScaleIntervals();
				std::uniform_int_distribution<int> dist(1, intervals.size());
				degree.setWithoutEventNotifications(dist(rng));
			}
			calculate();
		}));
		
		listeners.push(degree.newListener([this](int &){ calculate(); }));
		listeners.push(modeSelect.newListener([this](int &){
			currentModeState = modeSelect.get();
			calculate();
		}));
		listeners.push(userScale.newListener([this](vector<int> &){ calculate(); }));
		listeners.push(chordSize.newListener([this](int &){ calculate(); }));
		listeners.push(chordInterval.newListener([this](int &){ calculate(); }));
		listeners.push(deviationProb.newListener([this](float &){ calculate(); }));
		listeners.push(deviationRange.newListener([this](int &){ calculate(); }));
		listeners.push(majorProb.newListener([this](float &){ calculate(); }));
		
		currentModeState = modeSelect.get();
		calculate();
	}
	
private:
	// Axis System
	ofParameter<int> root;
	ofParameter<int> axis;
	ofParameter<int> pole;
	
	// Intervals
	ofParameter<int> axisInterval;
	ofParameter<int> poleInterval;
	
	// Scale / Mode
	ofParameter<int> modeSelect;
	ofParameter<vector<int>> userScale;
	
	// Mode Randomization
	ofParameter<bool> randModeOnAxis;
	ofParameter<bool> randModeOnPole;
	ofParameter<float> majorProb;
	int currentModeState = 0;  // Tracks current mode (randomized or base)
	
	// Chord
	ofParameter<int> degree;
	ofParameter<int> chordSize;
	ofParameter<int> chordInterval;
	
	// Randomization
	ofParameter<bool> randDegreeOnAxis;
	ofParameter<bool> randDegreeOnPole;
	ofParameter<bool> randPoleOnAxis;
	
	// Deviation
	ofParameter<float> deviationProb;
	ofParameter<int> deviationRange;
	
	// Outputs
	ofParameter<vector<int>> pitchOut;
	ofParameter<vector<int>> scaleOut;
	ofParameter<int> rootNoteOut;
	ofParameter<int> chordRootOut;
	ofParameter<int> currentModeOut;
	
	ofEventListeners listeners;
	customGuiRegion guiRegion;
	std::mt19937 rng;
	
	std::map<int, std::vector<int>> modeIntervals;
	const char* noteNames[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
	
	void initializeModes() {
		modeIntervals[0] = {0, 2, 4, 5, 7, 9, 11};  // Major
		modeIntervals[1] = {0, 2, 3, 5, 7, 8, 10};  // Minor
		modeIntervals[2] = {0, 2, 3, 5, 7, 9, 10};  // Dorian
		modeIntervals[3] = {0, 1, 3, 5, 7, 8, 10};  // Phrygian
		modeIntervals[4] = {0, 2, 4, 6, 7, 9, 11};  // Lydian
		modeIntervals[5] = {0, 2, 4, 5, 7, 9, 10};  // Mixolydian
		modeIntervals[6] = {0, 1, 3, 5, 6, 8, 10};  // Locrian
		modeIntervals[7] = {0, 2, 3, 5, 7, 8, 11};  // Harmonic Minor
		modeIntervals[8] = {0, 2, 4, 5, 7, 8, 11};  // Harmonic Major
		modeIntervals[9] = {0, 2, 3, 5, 7, 9, 11};  // Melodic Minor
	}
	
	void randomizeMode() {
		// Skip if user mode is selected
		if(modeSelect.get() == 10) return;
		
		// Randomly select between major (0) and minor (1) based on probability
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		if(dist(rng) < majorProb.get()) {
			currentModeState = 0;  // Major
		} else {
			currentModeState = 1;  // Minor
		}
	}
	
	std::vector<int> getCurrentScaleIntervals() {
		if(modeSelect.get() == 10) {
			std::vector<int> intervals;
			for(int val : userScale.get()) {
				intervals.push_back(val % 12);
			}
			if(intervals.empty()) {
				intervals = {0, 2, 4, 5, 7, 9, 11};
			}
			return intervals;
		}
		
		// Use randomized mode if mode randomization is enabled
		if(randModeOnAxis.get() || randModeOnPole.get()) {
			return modeIntervals[currentModeState];
		}
		
		return modeIntervals[modeSelect.get()];
	}
	
	// Axis order: 0=Tonic, 1=Subdominant, 2=Dominant
	int getAxisBase(int axisIndex) {
		int base = 0;
		switch(axisIndex) {
			case 0: base = 0; break;                              // Tonic
			case 1: base = 12 - axisInterval.get(); break;        // Subdominant
			case 2: base = axisInterval.get(); break;             // Dominant
		}
		return (base + root.get()) % 12;
	}
	
	int getPoleRoot(int axisIndex, int poleIndex) {
		int axisBase = getAxisBase(axisIndex);
		int poleOffset = (poleIndex * poleInterval.get()) % 12;
		return (axisBase + poleOffset) % 12;
	}
	
	int applyDeviation(int note) {
		if(deviationProb.get() <= 0.0f) return note;
		
		std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
		if(probDist(rng) < deviationProb.get()) {
			std::uniform_int_distribution<int> devDist(-deviationRange.get(), deviationRange.get());
			int deviation = devDist(rng);
			if(deviation == 0 && deviationRange.get() > 0) {
				deviation = (probDist(rng) < 0.5f) ? -1 : 1;
			}
			return note + deviation;
		}
		return note;
	}
	
	void calculate() {
		int poleRoot = getPoleRoot(axis.get(), pole.get());
		std::vector<int> intervals = getCurrentScaleIntervals();
		int numScaleNotes = intervals.size();
		
		// Build scale
		std::vector<int> scale;
		for(int i = 0; i < numScaleNotes; i++) {
			int note = poleRoot + intervals[i];
			scale.push_back(note);
		}
		
		// Calculate chord root (before deviation is applied)
		int degreeIndex = (degree.get() - 1) % numScaleNotes;
		int chordRoot = (poleRoot + intervals[degreeIndex]) % 12;
		
		// Build chord
		std::vector<int> chord;
		for(int i = 0; i < chordSize.get(); i++) {
			int scaleIndex = (degreeIndex + i * chordInterval.get()) % numScaleNotes;
			int octaveOffset = (degreeIndex + i * chordInterval.get()) / numScaleNotes;
			int note = poleRoot + intervals[scaleIndex] + (octaveOffset * 12);
			note = applyDeviation(note);
			chord.push_back(note);
		}
		
		pitchOut = chord;
		scaleOut = scale;
		rootNoteOut = poleRoot;
		chordRootOut = chordRoot;
		currentModeOut = currentModeState;
	}
	
	void drawAxisDisplay() {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float totalWidth = 260;
		float axisBoxSize = 76;
		float spacing = 8;
		float poleRadius = 7;
		float totalHeight = axisBoxSize + 36;
		
		ImGui::InvisibleButton("AxisDisplay", ImVec2(totalWidth, totalHeight));
		
		ImU32 inactiveAxisBg = IM_COL32(45, 45, 45, 255);
		ImU32 activeAxisBg = IM_COL32(180, 140, 30, 255);
		ImU32 inactivePoleBg = IM_COL32(65, 65, 65, 255);
		ImU32 activePoleBg = IM_COL32(70, 160, 200, 255);
		ImU32 textLight = IM_COL32(190, 190, 190, 255);
		ImU32 textDark = IM_COL32(25, 25, 25, 255);
		
		// Visual: S-T-D, Parameter: 0=T, 1=S, 2=D
		int visualToAxis[3] = {1, 0, 2};
		const char* axisLabels[3] = {"S", "T", "D"};
		
		for(int vis = 0; vis < 3; vis++) {
			int axisIdx = visualToAxis[vis];
			float boxX = pos.x + vis * (axisBoxSize + spacing);
			float boxY = pos.y;
			
			bool isActive = (axis.get() == axisIdx);
			ImU32 bgColor = isActive ? activeAxisBg : inactiveAxisBg;
			
			drawList->AddRectFilled(
				ImVec2(boxX, boxY),
				ImVec2(boxX + axisBoxSize, boxY + axisBoxSize),
				bgColor, 5.0f);
			
			// Axis letter
			const char* label = axisLabels[vis];
			ImVec2 labelSize = ImGui::CalcTextSize(label);
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.6f,
				ImVec2(boxX + (axisBoxSize - labelSize.x * 1.6f) / 2,
					   boxY + (axisBoxSize - labelSize.y * 1.6f) / 2),
				isActive ? textDark : IM_COL32(70, 70, 70, 255), label);
			
			// 4 poles in corners
			float margin = 12;
			float polePos[4][2] = {
				{boxX + margin, boxY + margin},
				{boxX + axisBoxSize - margin, boxY + margin},
				{boxX + margin, boxY + axisBoxSize - margin},
				{boxX + axisBoxSize - margin, boxY + axisBoxSize - margin}
			};
			
			for(int p = 0; p < 4; p++) {
				float px = polePos[p][0];
				float py = polePos[p][1];
				bool isPoleActive = isActive && (pole.get() == p);
				
				drawList->AddCircleFilled(ImVec2(px, py), poleRadius,
					isPoleActive ? activePoleBg : inactivePoleBg);
				
				int noteIdx = getPoleRoot(axisIdx, p);
				const char* noteName = noteNames[noteIdx];
				ImVec2 noteSize = ImGui::CalcTextSize(noteName);
				
				float tx = (p % 2 == 0) ? px + poleRadius + 2 : px - poleRadius - noteSize.x - 2;
				float ty = py - noteSize.y / 2;
				
				drawList->AddText(ImVec2(tx, ty), isActive ? textDark : textLight, noteName);
			}
		}
		
		// Info line
		float infoY = pos.y + axisBoxSize + 6;
		int currentRoot = getPoleRoot(axis.get(), pole.get());
		const char* modeNames[] = {"Maj", "Min", "Dor", "Phr", "Lyd", "Mix", "Loc", "HMin", "HMaj", "Mel", "Usr"};
		const char* axisName[] = {"T", "S", "D"};
		
		// Show current randomized mode if mode randomization is active
		const char* currentModeName = modeNames[modeSelect.get()];
		if((randModeOnAxis.get() || randModeOnPole.get()) && modeSelect.get() != 10) {
			currentModeName = modeNames[currentModeState];
		}
		
		char info[80];
		snprintf(info, sizeof(info), "%s %s | %s%d | deg%d |",
			noteNames[currentRoot], currentModeName,
			axisName[axis.get()], pole.get() + 1, degree.get());
		
		std::string fullInfo = info;
		for(size_t i = 0; i < pitchOut.get().size(); i++) {
			fullInfo += " " + std::to_string(pitchOut.get()[i]);
		}
		
		drawList->AddText(ImVec2(pos.x, infoY), textLight, fullInfo.c_str());
	}
};

#endif /* bartokAxis_h */
