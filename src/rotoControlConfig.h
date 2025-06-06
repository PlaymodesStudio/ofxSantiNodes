#ifndef rotoControlConfig_h
#define rotoControlConfig_h

#include "ofxOceanodeNodeModel.h"
#include "ofSerial.h"
#include "imgui.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeMidiBinding.h"
#include "ofThread.h"
#include <mutex>
/**
 * @class rotoControlConfig
 * @brief An Oceanode node for configuring ROTO-CONTROL display and controls
 *
 * This node allows setting name labels, background colors, MIDI channels,
 * CC numbers, and step counts for ROTO-CONTROL hardware knobs and switches.
 * It configures knobs and switches separately with proper page support.
 */
class rotoControlConfig : public ofxOceanodeNodeModel {
public:
	/**
	 * @brief Constructor
	 */
	rotoControlConfig() : ofxOceanodeNodeModel("ROTO-Control Config") {}
	
	//destructor
	~rotoControlConfig();


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
	void setContainer(ofxOceanodeContainer* container) override;

private:
	ofxOceanodeContainer* containerRef = nullptr;
	
	
	// Hardware constants based on API documentation
	static const int NUM_KNOBS_PER_PAGE = 8;    // 8 knobs per page
	static const int NUM_SWITCHES_PER_PAGE = 8; // 8 switches per page
	static const int NUM_PAGES = 2;             // 2 pages total
	static const int TOTAL_KNOBS = 32;          // 32 total knobs
	static const int TOTAL_SWITCHES = 32;       // 32 total switches
	static const int MAX_SETUPS = 64;

	// Control configuration structures
	struct KnobConfig {
		string name = "Knob";
		int color = 0;
		int midiChannel = 1;
		int midiCC = 0;
		int steps = 0;  // 0 = continuous, 2-10 = stepped
		bool configured = false;
	};

	struct SwitchConfig {
		string name = "Switch";
		int color = 0;  // LED ON color
		int midiChannel = 1;
		int midiCC = 0;
		bool configured = false;
		// LED OFF color is always black (70) per requirements
	};

	// For each of the 64 setups, store a full array of 32 KnobConfig and 32 SwitchConfig.
	// If a setup has never been configured, we can leave its 'exists' flag = false and skip saving its data.
	vector<vector<KnobConfig>> allKnobConfigs;     // size 64 × 32
	vector<vector<SwitchConfig>> allSwitchConfigs; // size 64 × 32

	// Page selection
	ofParameter<int> selectedPage;

	// Knob parameters (for the selected knob on current page)
	ofParameter<int> selectedKnob;
	ofParameter<string> knobName;
	ofParameter<int> knobMidiChannel;
	ofParameter<int> knobMidiCC;
	ofParameter<int> knobSteps;

	// Switch parameters (for the selected switch on current page)
	ofParameter<int> selectedSwitch;
	ofParameter<string> switchName;
	ofParameter<int> switchMidiChannel;
	ofParameter<int> switchMidiCC;
	
	// Configuration save/load using preset system
	   ofParameter<void> saveConfigButton;
	   ofParameter<void> loadConfigButton;
	
	// Threading for non-blocking serial operations
	std::thread configThread;
	std::atomic<bool> isConfiguring{false};
	std::mutex configMutex;

	  // Status display
	  ofParameter<string> configStatus;

	// Setup management
	struct SetupInfo {
		int index = 0;
		string name = "Setup";
		bool exists = false;
	};
	
	vector<SetupInfo> availableSetups;
	ofParameter<int> selectedSetupIndex;
	ofParameter<string> setupName;
	
	// Custom UI regions
	ofParameter<std::function<void()>> knobColorRegion;
	ofParameter<std::function<void()>> switchColorRegion;

	// Serial communication
	ofSerial serial;
	bool serialConnected = false;

	// Event listeners for parameter changes
	ofEventListeners listeners;
	bool ignoreListeners = false;
	
	

	// Helper functions
	void setupSerialPort();
	void closeSerialPort();
	void sendSerialCommand(unsigned char commandType, unsigned char subType, vector<unsigned char> payload = {});
	void setHardwarePage(int page);
	void applyKnobConfiguration(int knobIndex);
	void applySwitchConfiguration(int switchIndex);
	void updateSelectedKnobParameters();
	void updateSelectedSwitchParameters();
	void storeCurrentKnobSettings();
	void storeCurrentSwitchSettings();
	int getAbsoluteKnobIndex();
	int getAbsoluteSwitchIndex();
	void readSerialResponses();
	void onPageChanged();
	
	// Setup management functions
	void refreshAvailableSetups();
	void saveCurrentSetup();
	void loadSelectedSetup();
	void setSetupName(int setupIndex, const string& name);
	void getCurrentSetup();
	void setCurrentSetup(int setupIndex);
	
	// Sync methods
	void syncBoundParametersToHardware();
 shared_ptr<ofxOceanodeAbstractMidiBinding> findBindingForMidiMapping(int channel, int cc);
 void forceParameterResend(shared_ptr<ofxOceanodeAbstractMidiBinding> binding);
 void forceParameterResendByName(const string& moduleName, const string& paramName);
	string getRotoMidiPortName() const;
	
	// Helper methods for config save/load
	   void saveConfigurationFile();
	   void loadConfigurationFile();
	string getDefaultConfigDir();
	
	// Threaded operations
	  void threadedConfigurationApply();
	  void applyConfigurationsToHardware();
	  bool shouldSendSetupToHardware(int setupIndex);


};

#endif /* rotoControlConfig_h */
