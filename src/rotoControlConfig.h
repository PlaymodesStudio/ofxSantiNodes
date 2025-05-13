#ifndef rotoControlConfig_h
#define rotoControlConfig_h

#include "ofxOceanodeNodeModel.h"
#include "ofSerial.h"
#include "imgui.h" // Add this to include ImGui properly

/**
 * @class rotoControlConfig
 * @brief An Oceanode node for configuring ROTO-CONTROL display and controls
 *
 * This node allows setting name labels, background colors, MIDI channels,
 * CC numbers, and step counts for ROTO-CONTROL hardware knobs.
 * It configures one control at a time with an option to select which control.
 */
class rotoControlConfig : public ofxOceanodeNodeModel {
public:
	/**
	 * @brief Constructor
	 */
	rotoControlConfig() : ofxOceanodeNodeModel("ROTO-Control Config") {}
	
	/**
	 * @brief Setup initializes the node parameters
	 */
	void setup() override;
	
	/**
	 * @brief Update is called each frame
	 */
	void update(ofEventArgs &args) override;
	
	/**
	 * @brief Methods for preset handling
	 */
	void presetRecallBeforeSettingParameters(ofJson &json) override;
	void presetRecallAfterSettingParameters(ofJson &json) override;
	void presetSave(ofJson &json) override;
	
private:
	// Color definitions for the ROTO CONTROL
	struct RotoColor {
		std::string name;
		float r, g, b; // Use individual components instead of ImVec4
	};
	std::vector<RotoColor> rotoColors;
	void initializeColorMap();
	
	// Control configuration
	static const int NUM_PHYSICAL_CONTROLS = 16; // Physical controls on device
	static const int NUM_PAGES = 8;             // Number of pages
	static const int TOTAL_CONTROLS = NUM_PHYSICAL_CONTROLS * NUM_PAGES;
	
	// Control settings storage
	struct ControlConfig {
		string name = "Control";
		int color = 0;
		int midiChannel = 1;
		int midiCC = 0;
		int steps = 0;
		bool configured = false;
	};
	
	vector<ControlConfig> controlConfigs;
	
	// Currently selected control and page
	ofParameter<int> selectedControl;
	ofParameter<int> selectedPage;
	
	// Per-control parameters (for the selected control)
	ofParameter<string> controlName;
	ofParameter<int> controlColor;
	ofParameter<int> controlMidiChannel;
	ofParameter<int> controlMidiCC;
	ofParameter<int> controlSteps;
	
	// Serial communication
	ofSerial serial;
	bool serialConnected = false;
	
	// Custom UI region for preview
	ofParameter<std::function<void()>> previewRegion;
	
	// Event listeners for parameter changes
	ofEventListeners listeners;
	bool ignoreListeners = false;
	
	// Helper functions
	void setupSerialPort();
	void closeSerialPort();
	void sendSerialCommand(unsigned char commandType, unsigned char subType, vector<unsigned char> payload = {});
	void applyConfiguration();
	void applyControlConfiguration(int controlIndex);
	void drawControlPreview();
	void updateSelectedControlParameters();
	void storeCurrentControlSettings();
	int getAbsoluteControlIndex();
	void readSerialResponses();
};

#endif /* rotoControlConfig_h */
