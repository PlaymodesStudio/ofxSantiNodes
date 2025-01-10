#ifndef TTS_h
#define TTS_h

#include "ofxOceanodeNodeModel.h"
#include <future>
#include <thread>

class TTS : public ofxOceanodeNodeModel {
public:
    TTS() : ofxOceanodeNodeModel("Aina TTS") {
        containerStatus = false;
        dockerPath = "/usr/local/bin/docker";
        soxPath = "/opt/homebrew/bin/sox";
        triggerCounter = 0;
        triggerStartFrame = 0;
        writeInProgress = false;

    }
    
    void setup() {
        description = "Catalan Text-to-Speech node that generates natural sounding speech using different voices and accents. Requires minimal-tts-api Docker container, found here: https://github.com/langtech-bsc/minimal-tts-api/tree/wavenext_e2e; it also needs sox (brew install sox)";
        
        ofDirectory dir(ofToDataPath("tts", true));
        if(!dir.exists()){
            dir.create(true);
        }
        
        addParameter(inputText.set("Text", ""));
        addParameter(speed.set("Speed", 1.0, 0.5, 2.0));
        addParameterDropdown(accent, "Accent", 0, {"balear", "central", "nord-occidental", "valencia"});
        addParameterDropdown(voice, "Voice", 0, {"voice1", "voice2"});
        addParameter(temperature.set("Temperature", 0.7, 0.0, 1.0));

        addParameter(outputPath.set("Out Path", ""));
        addParameter(playButton.set("Play"));
        addParameter(writeButton.set("Write"));
        addParameter(containerActive.set("Docker", false));
        addParameter(containerStatusColor.set("Status", ofColor(0)));
        addParameter(lastGeneratedFile.set("File", ""));
        addOutputParameter(trigger.set("Trigger", 0, 0, 1));
        
        if(!ofFile::doesFileExist(soxPath)) {
            ofLogError("TTS") << "Sox not found at " << soxPath << ". Install with 'brew install sox'";
        }
        
        // Check for running container before cleanup
        if(checkContainerStatus()) {
            ofLogNotice("TTS") << "Found existing active container";
            containerStatus = true;
            containerActive = true;
            containerStatusColor.set(ofColor(0, 255, 0));
        } else if(ofFile::doesFileExist(dockerPath)) {
            cleanupExistingContainers();
        }
        
        listeners.push(playButton.newListener([this](){
            if(containerStatus) executeTTSPlay();
            else ofLogError("TTS") << "Docker container not active";
        }));
        
        listeners.push(writeButton.newListener([this](){
            if(containerStatus) executeTTSWrite();
            else ofLogError("TTS") << "Docker container not active";
        }));
        
        listeners.push(containerActive.newListener([this](bool& active){
            ofLogNotice("TTS") << "Container toggle: " << (active ? "ON" : "OFF");
            if(active && !containerStatus) activateContainer();
            else if(!active && containerStatus) deactivateContainer();
        }));
        
        
    }
    
    void update(ofEventArgs &a) override {
            if (writeInProgress && writeFuture.valid()) {
                auto status = writeFuture.wait_for(std::chrono::seconds(0));
                if (status == std::future_status::ready) {
                    bool success = writeFuture.get();
                    if (success) {
                        triggerCounter = 15;
                        trigger.set(1);
                        ofLogNotice("TTS") << "Write completed successfully";
                    }
                    writeInProgress = false;
                }
            }

            if(triggerCounter > 0) {
                triggerCounter--;
                if(triggerCounter == 0) {
                    trigger.set(0);
                }
            }
        }
    
private:
    
    string urlEncode(const string& str) {
        string encoded;
        for (char c : str) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                encoded += hex;
            }
        }
        return encoded;
    }
    
    string escapeJSON(const string& input) {
        string output;
        for(char c : input) {
            if (c == '"' || c == '\'') {
                output += "\\";
            }
            output += c;
        }
        return output;
    }
    
    bool checkContainerStatus() {
            // First check if container exists and is running
            string checkCmd = dockerPath + " ps | grep minimal-tts-api";
            int result = system((checkCmd + " >/dev/null 2>&1").c_str());
            
            if(result != 0) {
                ofLogError("TTS") << "Container not running";
                return false;
            }
            
            // Then check if service is responding
            ofSleepMillis(1000); // Give it a moment
            
            string curlCmd = "curl -s http://127.0.0.1:8000/api/tts -o /dev/null";
            result = system((curlCmd + " >/dev/null 2>&1").c_str());
            
            ofLogNotice("TTS") << "Service check result: " << result;
            return (result == 0);
        }
    
    bool isPortAvailable() {
            string cmd = "lsof -i :8000";
            FILE* pipe = popen(cmd.c_str(), "r");
            char buffer[128];
            string result = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    result += buffer;
            }
            int status = pclose(pipe);
            return result.empty(); // If empty, port is available
        }
        
    void cleanupExistingContainers() {
            string findCmd = dockerPath + " ps -a | grep minimal-tts-api | awk '{print $1}'";
            FILE* pipe = popen(findCmd.c_str(), "r");
            char buffer[128];
            string containerId = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    containerId += buffer;
            }
            pclose(pipe);
            
            containerId = containerId.substr(0, containerId.find_last_not_of("\n\r") + 1);
            
            if(!containerId.empty()) {
                ofLogNotice("TTS") << "Cleaning up container: " << containerId;
                
                string stopCmd = dockerPath + " stop " + containerId;
                system((stopCmd + " >/dev/null 2>&1").c_str());
                
                string rmCmd = dockerPath + " rm " + containerId;
                system((rmCmd + " >/dev/null 2>&1").c_str());
            }
        }
    
    void activateContainer() {
            ofLogNotice("TTS") << "Activating container...";
            
            if(!ofFile::doesFileExist(dockerPath)) {
                ofLogError("TTS") << "Docker not found at " << dockerPath;
                containerActive = false;
                containerStatusColor.set(ofColor(255, 0, 0));
                return;
            }
            
            cleanupExistingContainers();
            
            // Start container
            string cmd = dockerPath + " run -d -p 8000:8000 -t minimal-tts-api";
            int result = system((cmd + " >/dev/null 2>&1").c_str());
            
            ofLogNotice("TTS") << "Container start result: " << result;
            
            // Wait and check status
            ofSleepMillis(3000); // Give more time to start
            
            containerStatus = checkContainerStatus();
            containerStatusColor.set(containerStatus ? ofColor(0, 255, 0) : ofColor(255, 0, 0));
            
            ofLogNotice("TTS") << "Container status: " << (containerStatus ? "ACTIVE" : "FAILED");
            
            if(!containerStatus) {
                containerActive = false;
                ofLogError("TTS") << "Failed to start Docker container";
            }
        }
        
        void deactivateContainer() {
            ofLogNotice("TTS") << "Deactivating container...";
            
            if(!ofFile::doesFileExist(dockerPath)) {
                return;
            }
            
            cleanupExistingContainers();
            containerStatus = false;
            containerStatusColor.set(ofColor(0));
        }

    string getCleanerForAccent(int accentIndex) {
           vector<string> cleaners = {
               "catalan_balear_cleaners",
               "catalan_cleaners",
               "catalan_occidental_cleaners",
               "catalan_valencia_cleaners"
           };
           return cleaners[accentIndex];
       }

    string getVoiceForAccent(int accentIndex, int voiceIndex) {
          vector<vector<string>> voices = {
              {"quim", "olga"},
              {"grau", "elia"},
              {"pere", "emma"},
              {"lluc", "gina"}
          };
          return voices[accentIndex][voiceIndex];
      }

    void executeTTSPlay() {
        if(inputText.get().empty()) {
            ofLogWarning("TTS") << "No text specified";
            return;
        }
        
        ofLogNotice("TTS") << "Executing TTS Play...";
        
        string tempFile = ofToDataPath("tts/temp_tts.wav", true);
        string tempFile441 = ofToDataPath("tts/temp_tts_441.wav", true);
        string tempJson = ofToDataPath("tts/temp.json", true);
        
        // Create JSON file
        string jsonContent = "{\"text\":\"" + inputText.get() + "\","
                            "\"voice\":\"" + getVoiceForAccent(accent.get(), voice.get()) + "\","
                            "\"accent\":\"" + vector<string>{"balear", "central", "nord-occidental", "valencia"}[accent.get()] + "\","
                            "\"type\":\"text\","
                            "\"length_scale\":" + ofToString(speed.get()) + ","
                            "\"temperature\":" + ofToString(temperature.get()) + ","
                            "\"cleaner\":\"" + getCleanerForAccent(accent.get()) + "\"}";


        
        ofFile jsonFile(tempJson, ofFile::WriteOnly);
        jsonFile.write(jsonContent.c_str(), jsonContent.length());
        jsonFile.close();
        
        string cmdCreate = "curl -X POST http://127.0.0.1:8000/api/tts "
                         "-H \"Content-Type: application/json\" "
                         "-d @\"" + tempJson + "\" "
                         "> \"" + tempFile + "\" && "
                         + soxPath + " \"" + tempFile + "\" -r 44100 \"" + tempFile441 + "\" && "
                         "rm \"" + tempFile + "\" \"" + tempJson + "\"";
        
        ofLogNotice("TTS") << "Executing curl command...";
        int result = system(cmdCreate.c_str());
        ofLogNotice("TTS") << "Curl result: " << result;
        
        if(result == 0) {
            lastGeneratedFile.set(tempFile441);
            
            string cmdPlay = "afplay \"" + tempFile441 + "\"";
            system(cmdPlay.c_str());
            
            string cmdCleanup = "rm \"" + tempFile441 + "\"";
            system(cmdCleanup.c_str());
        } else {
            ofLogError("TTS") << "Failed to generate audio";
        }
    }

    void executeTTSWrite() {
            if(inputText.get().empty()) {
                ofLogWarning("TTS") << "No text specified";
                return;
            }
            
            if (writeInProgress) {
                ofLogWarning("TTS") << "Write operation already in progress";
                return;
            }
            
            writeInProgress = true;
            writeFuture = std::async(std::launch::async, [this]() {
                ofLogNotice("TTS") << "Executing TTS Write...";
                
                string timestamp = ofGetTimestampString();
                string tempFile = ofToDataPath("tts/temp_tts.wav", true);
                string outputFile = ofToDataPath("tts/tts_" + timestamp + ".wav", true);
                string tempJson = ofToDataPath("tts/temp.json", true);
                
                string jsonContent = "{\"text\":\"" + inputText.get() + "\","
                                    "\"voice\":\"" + getVoiceForAccent(accent.get(), voice.get()) + "\","
                                    "\"accent\":\"" + vector<string>{"balear", "central", "nord-occidental", "valencia"}[accent.get()] + "\","
                                    "\"type\":\"text\","
                                    "\"length_scale\":" + ofToString(speed.get()) + ","
                                    "\"temperature\":" + ofToString(temperature.get()) + ","
                                    "\"cleaner\":\"" + getCleanerForAccent(accent.get()) + "\"}";
                
                ofFile jsonFile(tempJson, ofFile::WriteOnly);
                jsonFile.write(jsonContent.c_str(), jsonContent.length());
                jsonFile.close();
                
                string cmd = "curl -X POST http://127.0.0.1:8000/api/tts "
                           "-H \"Content-Type: application/json\" "
                           "-d @\"" + tempJson + "\" "
                           "> \"" + tempFile + "\" && "
                           + soxPath + " \"" + tempFile + "\" -r 44100 \"" + outputFile + "\" && "
                           "rm \"" + tempFile + "\" \"" + tempJson + "\"";
                
                int result = system(cmd.c_str());
                
                if(result == 0) {
                    lastGeneratedFile.set(outputFile);
                    ofLogNotice("TTS") << "File saved: " + outputFile;
                    return true;
                } else {
                    ofLogError("TTS") << "Failed to save file";
                    return false;
                }
            });
            
            return;
        }

    ofParameter<string> inputText;
        ofParameter<float> speed;
        ofParameter<int> accent;
        ofParameter<int> voice;
        ofParameter<string> outputPath;
        ofParameter<void> playButton;
        ofParameter<void> writeButton;
        ofParameter<bool> containerActive;
        ofParameter<string> lastGeneratedFile;
        ofParameter<ofColor> containerStatusColor;
        ofParameter<int> trigger;
    ofParameter<float> temperature;

    
        
        bool containerStatus;
    bool writeInProgress;
    std::future<bool> writeFuture;

        string dockerPath;
        string soxPath;
        int triggerCounter;
    int triggerStartFrame;

        ofEventListeners listeners;
    };

#endif /* TTS_h */
