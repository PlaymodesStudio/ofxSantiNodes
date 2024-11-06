#ifndef RadioStation_h
#define RadioStation_h

#include "ofxOceanodeNodeModel.h"
#include "ofJson.h"

class RadioStation : public ofxOceanodeNodeModel, public ofThread {
public:
    RadioStation() : ofxOceanodeNodeModel("Radio Station") {
        pythonEnvReady = false;
        pendingDeviceSetup = false;
        setupFrameDelay = 0;


            
            // Create radio directory if it doesn't exist
            radioDir = ofToDataPath("radio", true);
            ofDirectory dir(radioDir);
            if(!dir.exists()){
                dir.create(true);
            }
            
            // First, load stations from JSON
            loadStationsFromJson();
            
            // Then, setup Python and load devices synchronously
            if (setupPythonEnv()) {
                pythonEnvReady = true;
                
                // Kill any existing daemons
                system("pkill -f \"radio.py --daemon\"");
                if(ofFile::doesFileExist("/tmp/radio.sock")) {
                    ofFile::removeFile("/tmp/radio.sock");
                }
                
                // Start the daemon
                string radioPyPath = ofToDataPath("radio/radio.py", true);
                string cmd = getPythonCmd() + " \"" + radioPyPath + "\" --daemon &";
                system(cmd.c_str());
                
                // Wait for daemon to start
                int retries = 10;
                while(retries > 0) {
                    if(checkDaemon()) break;
                    ofSleepMillis(500);
                    retries--;
                }
                
                // Load devices
                loadAudioDevices();
            } else {
                // Set default values if Python setup fails
                audioDeviceNames = {"System Default"};
                deviceData = {{"System Default", "-1"}};
            }
            
            // Now set up parameters with the loaded data
            setupParameters();
        
        ofAddListener(parameterGroupChanged, this, &RadioStation::onParameterGroupChanged);
        ofAddListener(ofEvents().update, this, &RadioStation::update);


            
            // Start monitoring thread
            startThread();
        }
    
    
    ~RadioStation() {
            ofRemoveListener(parameterGroupChanged, this, &RadioStation::onParameterGroupChanged);
            ofRemoveListener(ofEvents().update, this, &RadioStation::update);
            if(isThreadRunning()) {
                threadShouldStop = true;
                waitForThread(true, 3000);
            }
        }
    
    void onParameterGroupChanged() {
        if(!pendingDeviceSetup) {
            pendingDeviceSetup = true;
            setupFrameDelay = 60; // Wait ~1 second (assuming 60fps)
            ofLogNotice("RadioStation") << "Scheduling audio device setup";
        }
    }
    
    void update(ofEventArgs &args) {
        if(pendingDeviceSetup && setupFrameDelay > 0) {
            setupFrameDelay--;
            if(setupFrameDelay == 0) {
                if(ensurePythonEnvironment()) {
                    // First set the audio device
                    if(audioDevice.get() < deviceData.size()) {
                        ofLogNotice("RadioStation") << "Applying saved audio device setting";
                        setAudioDevice(deviceData[audioDevice.get()].id);
                    }
                    
                    // Then start playback if station is selected
                    if(station.get() < stationNames.size()) {
                        ofLogNotice("RadioStation") << "Restarting playback of saved station";
                        play();
                    }
                } else {
                    status.set("Failed to setup Python environment");
                }
                pendingDeviceSetup = false;
            }
        }
    }
    
    void threadedFunction() {
            while(isThreadRunning() && !threadShouldStop) {
                if(pythonEnvReady && !checkDaemon()) {
                    // Restart daemon if it died
                    startDaemon();
                    
                    // Reapply audio device setting after daemon restart
                    if(audioDevice.get() < deviceData.size()) {
                        setAudioDevice(deviceData[audioDevice.get()].id);
                    }
                }
                ofSleepMillis(1000);
            }
        }
    
    void exit() {
            // Make sure to cleanup when node is removed
            if(isThreadRunning()) {
                threadShouldStop = true;
                waitForThread(true, 3000);
            }
        }
    
    void loadStationsFromJson() {
            string stationsPath = ofToDataPath("radio/stations.json", true);
            if (ofFile::doesFileExist(stationsPath)) {
                try {
                    ofJson stations = ofLoadJson(stationsPath);
                    stationNames.clear();
                    
                    vector<string> sortedKeys;
                    for (auto& [key, value] : stations.items()) {
                        sortedKeys.push_back(key);
                    }
                    std::sort(sortedKeys.begin(), sortedKeys.end());
                    
                    for (const auto& key : sortedKeys) {
                        stationNames.push_back(key);
                    }
                } catch (const std::exception& e) {
                    ofLogError("RadioStation") << "Error loading stations: " << e.what();
                    stationNames = {"Error loading stations"};
                }
            }
        }
    
    void loadInitialData() {
        // Create radio directory if it doesn't exist
        radioDir = ofToDataPath("radio", true);
        ofDirectory dir(radioDir);
        if(!dir.exists()){
            dir.create(true);
        }
        
        // Initialize with safe defaults first
        stationNames = {"Loading stations..."};
        audioDeviceNames = {"System Default"};
        deviceData = {{"System Default", "-1"}};
        
        // Only load stations from JSON - no Python commands yet
        string stationsPath = ofToDataPath("radio/stations.json", true);
        if (ofFile::doesFileExist(stationsPath)) {
            try {
                ofJson stations = ofLoadJson(stationsPath);
                stationNames.clear();
                
                vector<string> sortedKeys;
                for (auto& [key, value] : stations.items()) {
                    sortedKeys.push_back(key);
                }
                std::sort(sortedKeys.begin(), sortedKeys.end());
                
                for (const auto& key : sortedKeys) {
                    stationNames.push_back(key);
                }
            } catch (const std::exception& e) {
                ofLogError("RadioStation") << "Error loading stations: " << e.what();
                stationNames = {"Error loading stations"};
            }
        }
        
        // Do NOT try to load devices here - we'll do it after Python env is ready
    }
       
    void initialize() {
            // Setup Python environment
            if (setupPythonEnv()) {
                pythonEnvReady = true;
                
                // Now that Python is ready, load audio devices
                loadAudioDevices();
                status.set("Ready");
            } else {
                ofLogError("RadioStation") << "Failed to initialize Python environment";
                status.set("Failed to initialize Python environment");
            }
            
            setupParameters();
        }
    
    void setup() {
        description = "Radio player daemon that streams internet radio to different audio outputs";
    }
    
    void setupParameters() {
           // Add parameters
           addParameter(status.set("Status", "Initializing..."));
           addParameter(daemonActive.set("Daemon Active", false));
           addParameter(daemonStatusColor.set("Daemon Status", ofColor(0)));
           addParameter(volume.set("Volume", 100, 0, 100));
           
           // Add dropdowns with initial data
           addParameterDropdown(station, "Station", 0, stationNames);
           addParameterDropdown(audioDevice, "Audio Device", 0, audioDeviceNames);
           
           // Add buttons
           addParameter(playButton.set("Play"));
           addParameter(stopButton.set("Stop"));
           
           // Setup listeners
           listeners.unsubscribeAll();
           
           listeners.push(playButton.newListener([this](){
               if(!pythonEnvReady) {
                   status.set("Python environment not ready");
                   return;
               }
               if(checkDaemon()) play();
               else ofLogError("RadioDaemon") << "Daemon not active";
           }));
           
           listeners.push(stopButton.newListener([this](){
               if(!pythonEnvReady) {
                   status.set("Python environment not ready");
                   return;
               }
               if(checkDaemon()) stop();
               else ofLogError("RadioDaemon") << "Daemon not active";
           }));
           
           listeners.push(volume.newListener([this](float& val){
               if(!pythonEnvReady) {
                   status.set("Python environment not ready");
                   return;
               }
               if(checkDaemon()) setVolume(val);
           }));
           
           listeners.push(audioDevice.newListener([this](int& val){
               if(!pythonEnvReady) {
                   status.set("Python environment not ready");
                   return;
               }
               if(checkDaemon() && val < deviceData.size()) {
                   setAudioDevice(deviceData[val].id);
               }
           }));
           
           listeners.push(daemonActive.newListener([this](bool& active){
               if(!pythonEnvReady) {
                   status.set("Python environment not ready");
                   daemonActive = false;
                   return;
               }
               ofLogNotice("RadioDaemon") << "Daemon toggle: " << (active ? "ON" : "OFF");
               if(active) startDaemon();
               else stopDaemon();
           }));
       }
    
    void refreshStationsAndDevices() {
        loadStations();
        loadAudioDevices();
    }

private:
    struct DeviceInfo {
        string name;
        string id;  // Keep as string to preserve exact ID
    };
    
        bool pythonEnvReady;
        bool threadShouldStop;
        bool pendingDeviceSetup;
        int setupFrameDelay;

        string radioDir;
        vector<string> stationNames;
        vector<string> audioDeviceNames;
        vector<DeviceInfo> deviceData;
        
        ofParameter<int> station;
        ofParameter<int> audioDevice;
        ofParameter<float> volume;
        ofParameter<void> playButton;
        ofParameter<void> stopButton;
        ofParameter<bool> daemonActive;
        ofParameter<string> status;
        ofParameter<ofColor> daemonStatusColor;
        
        ofEventListeners listeners;
    
    bool ensurePythonEnvironment() {
        if (!pythonEnvReady || !verifyPythonEnv()) {
            ofLogNotice("RadioStation") << "Python environment needs setup on preset load";
            if (setupPythonEnv()) {
                pythonEnvReady = true;
                
                // Restart daemon with new environment
                system("pkill -f \"radio.py --daemon\"");
                if(ofFile::doesFileExist("/tmp/radio.sock")) {
                    ofFile::removeFile("/tmp/radio.sock");
                }
                
                string radioPyPath = ofToDataPath("radio/radio.py", true);
                string cmd = getPythonCmd() + " \"" + radioPyPath + "\" --daemon &";
                system(cmd.c_str());
                
                // Wait for daemon to start
                int retries = 10;
                while(retries > 0) {
                    if(checkDaemon()) {
                        ofLogNotice("RadioStation") << "Daemon restarted after environment setup";
                        return true;
                    }
                    ofSleepMillis(500);
                    retries--;
                }
                return false;
            }
            return false;
        }
        return true;
    }
    
    string getPythonCmd() {
        string venvPath = ofToDataPath("radio/venv", true);
        #ifdef TARGET_WIN32
            return "\"" + venvPath + "/Scripts/python.exe\"";
        #else
            // Set up complete environment including VLC library path
            string vlcPath = "/Applications/VLC.app/Contents/MacOS/lib";
            
            // Create a script that sets up the environment and runs the command
            string scriptPath = ofToDataPath("radio/run_python.sh", true);
            string scriptContent = "#!/bin/bash\n"
                                 "export PYTHONPATH=\"" + venvPath + "/lib/python3.9/site-packages:$PYTHONPATH\"\n"
                                 "export DYLD_LIBRARY_PATH=\"" + vlcPath + ":$DYLD_LIBRARY_PATH\"\n"
                                 "export VLC_PLUGIN_PATH=\"/Applications/VLC.app/Contents/MacOS/plugins\"\n"
                                 "source \"" + venvPath + "/bin/activate\"\n"
                                 "\"" + venvPath + "/bin/python3\" \"$@\"";
            
            ofFile script(scriptPath, ofFile::WriteOnly);
            script.write(scriptContent.c_str(), scriptContent.length());
            script.close();
            
            // Make the script executable
            system(("chmod +x \"" + scriptPath + "\"").c_str());
            
            return scriptPath;
        #endif
    }
    
    bool executeCommand(const string& cmd, bool silent = false) {
           if (!pythonEnvReady) {
               if (!silent) status.set("Python environment not ready");
               return false;
           }
           return system((cmd + (silent ? " >/dev/null 2>&1" : "")).c_str()) == 0;
       }
    
    bool checkDaemon() {
        string socketPath = "/tmp/radio.sock";
        return ofFile::doesFileExist(socketPath);
    }
    
    bool verifyPythonEnv() {
            string venvPath = ofToDataPath("radio/venv", true);
            if (!ofDirectory(venvPath).exists()) {
                return false;
            }
            
            string testCmd = getPythonCmd() + " -c \"import vlc\"";
            if (system((testCmd + " >/dev/null 2>&1").c_str()) != 0) {
                return false;
            }
            
            return true;
        }
        
        void stopDaemon() {
            // Send stop-all command before killing daemon
            string cmd = "python3 \"" + ofToDataPath("radio/radio.py", true) + "\" stop-all";
            system(cmd.c_str());
            
            // Find and kill daemon process
            cmd = "pkill -f \"radio.py --daemon\"";
            system(cmd.c_str());
            
            status.set("Daemon stopped");
            daemonStatusColor.set(ofColor(0));
        }
    
    bool setupPythonEnv() {
        ofLogNotice("RadioStation") << "Setting up Python environment...";
        status.set("Setting up Python environment...");
        
        string venvPath = ofToDataPath("radio/venv", true);
        
        // Create new venv if needed
        if (!ofDirectory(venvPath).exists()) {
            string createVenvCmd = "python3 -m venv \"" + venvPath + "\"";
            ofLogNotice("RadioStation") << "Creating venv with command: " << createVenvCmd;
            
            if (system(createVenvCmd.c_str()) != 0) {
                status.set("Failed to create virtual environment");
                return false;
            }
            
            // Install required packages with specific version
            string pipCmd = "source \"" + venvPath + "/bin/activate\" && \"" + venvPath + "/bin/pip3\"";
            string installCmd = pipCmd + " install python-vlc==3.0.18122 requests --no-cache-dir";
            ofLogNotice("RadioStation") << "Installing packages with command: " << installCmd;
            
            if(system(installCmd.c_str()) != 0) {
                status.set("Failed to install Python packages");
                return false;
            }
        }
        
        // Verify installation with all environment variables set
        string testCmd = getPythonCmd() + " -c \"import vlc; print(vlc.__file__)\"";
        if(system((testCmd + " >/dev/null 2>&1").c_str()) != 0) {
            ofLogNotice("RadioStation") << "VLC import failed, removing virtual environment";
            ofDirectory(venvPath).remove(true);
            status.set("Failed to verify Python packages");
            return false;
        }
        
        ofLogNotice("RadioStation") << "Python environment setup complete";
        return true;
    }
    
    void loadRealData() {
        // Load stations
        string stationsPath = ofToDataPath("radio/stations.json", true);
        if (ofFile::doesFileExist(stationsPath)) {
            try {
                ofJson stations = ofLoadJson(stationsPath);
                stationNames.clear();
                
                vector<string> sortedKeys;
                for (auto& [key, value] : stations.items()) {
                    sortedKeys.push_back(key);
                }
                std::sort(sortedKeys.begin(), sortedKeys.end());
                
                for (const auto& key : sortedKeys) {
                    stationNames.push_back(key);
                }
            } catch (const std::exception& e) {
                ofLogError("RadioStation") << "Error loading stations: " << e.what();
                stationNames = {"Error loading stations"};
            }
        }
        
        // Load devices
        string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" devices";
        string result = ofSystem(cmd);
        
        try {
            ofJson response = ofJson::parse(result);
            if (response["status"] == "success" && response.contains("devices")) {
                audioDeviceNames.clear();
                deviceData.clear();
                
                audioDeviceNames.push_back("System Default");
                deviceData.push_back({"System Default", "-1"});
                
                auto& devices = response["devices"];
                for (auto& [index, device] : devices.items()) {
                    if(device.contains("name") && device.contains("use_this_id")) {
                        string deviceName = device["name"].get<string>();
                        string deviceId = device["use_this_id"].get<string>();
                        audioDeviceNames.push_back(deviceName);
                        deviceData.push_back({deviceName, deviceId});
                    }
                }
            }
        } catch (const std::exception& e) {
            ofLogError("RadioStation") << "Error loading devices: " << e.what();
        }
    }
    
    void startDaemon() {
            if (!pythonEnvReady) {
                status.set("Cannot start daemon - Python environment not ready");
                daemonStatusColor.set(ofColor(255, 0, 0));
                daemonActive = false;
                return;
            }
            
            // Kill any existing daemons first
            system("pkill -f \"radio.py --daemon\"");
            if(ofFile::doesFileExist("/tmp/radio.sock")) {
                ofFile::removeFile("/tmp/radio.sock");
            }
            
            string radioPyPath = ofToDataPath("radio/radio.py", true);
            string cmd = getPythonCmd() + " \"" + radioPyPath + "\" --daemon &";
            
            if(executeCommand(cmd, true)) {
                ofSleepMillis(1000);
                int retries = 5;
                while(retries > 0) {
                    if(checkDaemon()) {
                        status.set("Daemon started");
                        daemonStatusColor.set(ofColor(0, 255, 0));
                        daemonActive = true;
                        return;
                    }
                    ofSleepMillis(500);
                    retries--;
                }
            }
            
            status.set("Failed to start daemon");
            daemonStatusColor.set(ofColor(255, 0, 0));
            daemonActive = false;
        }
    
 
    void loadStations() {
        string stationsPath = ofToDataPath("radio/stations.json", true);
        stationNames.clear();
        
        if (ofFile::doesFileExist(stationsPath)) {
            try {
                ofJson stations = ofLoadJson(stationsPath);
                
                // Sort stations to maintain consistent order
                vector<string> sortedKeys;
                for (auto& [key, value] : stations.items()) {
                    sortedKeys.push_back(key);
                }
                std::sort(sortedKeys.begin(), sortedKeys.end());
                
                // Add stations in sorted order
                for (const auto& key : sortedKeys) {
                    stationNames.push_back(key);
                }
                
                // Update the parameter range
                if(station.getMax() != stationNames.size() - 1) {
                    station.setMax(stationNames.size() - 1);
                }
                
            } catch (const std::exception& e) {
                ofLogError("RadioStation") << "Error loading stations: " << e.what();
                stationNames = {"Error loading stations"};
            }
        }
    }
    
    void play() {
        if(!ensurePythonEnvironment()) {
            status.set("Failed to setup Python environment");
            return;
        }

        if(station >= stationNames.size()) {
            status.set("No valid station selected");
            return;
        }
        
        string selectedStation = stationNames[station];
        string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" play \"" + selectedStation + "\"";
        
        string result = ofSystem(cmd);
        
        try {
            ofJson response = ofJson::parse(result);
            if(response["status"] == "success") {
                status.set("Playing " + selectedStation);
            } else {
                status.set("Failed to play: " + response.value("message", "Unknown error"));
            }
        } catch(...) {
            status.set("Error playing station");
        }
    }
    
    void stop() {
            string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" stop";
            ofSystem(cmd);
            status.set("Playback stopped");
        }
        
        void setVolume(float vol) {
            string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" volume " + ofToString(vol);
            ofSystem(cmd);
        }
        
    void setAudioDevice(const string& deviceId) {
        if (!ensurePythonEnvironment()) {
            status.set("Failed to setup Python environment");
            return;
        }

        // Stop the daemon if it's running
        if (checkDaemon()) {
            stopDaemon();  // Custom function to stop the daemon
            ofSleepMillis(1000);  // Optional delay to ensure complete stop
        }

        // Set the new audio device
        string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" set-device \"" + deviceId + "\"";
        string result = ofSystem(cmd);

        try {
            ofJson response = ofJson::parse(result);
            if (response["status"] == "success") {
                status.set("Audio device changed: " + response["message"].get<string>());

                // Restart the daemon after changing the device
                startDaemon();  // Custom function to start the daemon
            } else {
                status.set("Failed to change device: " + response["message"].get<string>());
            }
        } catch (const std::exception& e) {
            ofLogError("RadioStation") << "Error setting device: " << e.what();
            status.set("Error setting audio device");
        }
    }

        
    void loadAudioDevices() {
            // Always start with system default
            audioDeviceNames = {"System Default"};
            deviceData = {{"System Default", "-1"}};
            
            string cmd = getPythonCmd() + " \"" + ofToDataPath("radio/radio.py", true) + "\" devices";
            string result = ofSystem(cmd);
            
            try {
                ofJson response = ofJson::parse(result);
                if (response["status"] == "success" && response.contains("devices")) {
                    auto& devices = response["devices"];
                    for (auto& [index, device] : devices.items()) {
                        if(device.contains("name") && device.contains("use_this_id")) {
                            string deviceName = device["name"].get<string>();
                            string deviceId = device["use_this_id"].get<string>();
                            
                            audioDeviceNames.push_back(deviceName);
                            deviceData.push_back({deviceName, deviceId});
                        }
                    }
                }
            } catch (const std::exception& e) {
                ofLogError("RadioStation") << "Error loading devices: " << e.what();
            }
        }

        
};

#endif /* RadioStation_h */
