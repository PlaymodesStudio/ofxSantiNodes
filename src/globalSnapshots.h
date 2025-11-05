#ifndef GLOBAL_SNAPSHOTS_H
#define GLOBAL_SNAPSHOTS_H

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeParameter.h"
#include <map>
#include <string>
#include <functional>
#include <unordered_set>
#include <set>
#include <vector>

class globalSnapshots : public ofxOceanodeNodeModel {
public:
	globalSnapshots();
	void setup() override;
	void setup(std::string additionalInfo) override;
	void update(ofEventArgs &e) override;
	void setContainer(ofxOceanodeContainer* c) override;

private:
	// Pointer to the patch container (set in setContainer)
	ofxOceanodeContainer* globalContainer = nullptr;

	// Node GUI and inspector parameters
	ofParameter<int>    activeSnapshotSlot;
	ofParameter<void>   addSnapshotButton;
	ofParameter<float>  interpolationMs;
	
	// Custom GUI regions
	customGuiRegion     snapshotControlGui;
	customGuiRegion     snapshotInspector;

	ofParameter<bool>   includeMacroParams;
	ofParameter<int>    matrixRows, matrixCols;
	ofParameter<float>  buttonSize;
	ofParameter<bool>   showSnapshotNames;

	// In‐memory snapshot storage
	struct ParameterSnapshot {
		std::string type;
		ofJson      value;
	};
	struct SnapshotData {
		std::string                                       name;
		std::map<std::string,ParameterSnapshot>           paramValues;
	};
	std::map<int,SnapshotData>  snapshots;
	int                          currentSnapshotSlot;
	
	// Interpolation state
	bool                        isInterpolating;
	float                       interpolationStartTime;
	int                         interpolationTargetSlot;
	std::map<std::string, ParameterSnapshot> interpolationStartValues;

	// Only these keys will be interpolated (i.e. values that actually differ)
	std::unordered_set<std::string> interpolationActiveKeys;

	// Manual blacklist of parameters (Group/Param strings)
	std::set<std::string> manualExcludes;

	// Event listeners
	ofEventListener              addSnapshotListener;
	ofEventListener              activeSnapshotSlotListener;

	// Core logic
	void storeSnapshot(int slot);
	void loadSnapshot(int slot);
	void startInterpolation(int targetSlot);
	void updateInterpolation();
	
	// Persistence
	void saveSnapshotsToFile();
	void loadSnapshotsFromFile();
	std::string getSnapshotsFilePath();

	// GUI rendering
	void renderSnapshotMatrix();
	void renderInspectorInterface();

	// Helpers
	bool isParameterModulated(const std::string& key) const;
	// full check: manual blacklist + modulated + flagged-as-output (DisableInConnection)
	bool isParameterExcluded(const std::string& key, ofxOceanodeAbstractParameter* param) const;
};

#endif // GLOBAL_SNAPSHOTS_H
