#ifndef Catotron_h
#define Catotron_h

#include "ofxOceanodeNodeModel.h"
#include <future>
#include <thread>
#include <iomanip>
#include <sstream>

class Catotron : public ofxOceanodeNodeModel {
public:
    Catotron() : ofxOceanodeNodeModel("Catotron TTS") {
            containerStatus = false;
            dockerPath = "/usr/local/bin/docker";
            soxPath = "/opt/homebrew/bin/sox";
            triggerCounter = 0;
            triggerStartFrame = 0;
            writeInProgress = false;
            currentFileIndex = 0;
            maxFiles = 20;
            
            // Set docker compose directory to data/catotron
            dockerComposeDir = ofToDataPath("catotron", true);
        }
    
    void setup() {
            description = "Catalan Text-to-Speech node that generates natural sounding speech using Catotron. Requires catotron-api Docker container and sox (brew install sox)";
            
            // Check if catotron directory exists
            ofDirectory dir(dockerComposeDir);
            if(!dir.exists()){
                ofLogError("Catotron") << "Catotron directory not found at: " << dockerComposeDir;
                ofLogError("Catotron") << "Please copy the tts-api contents to: " << dockerComposeDir;
            }
            
            // Create tts output directory if it doesn't exist
            ofDirectory ttsDir(ofToDataPath("tts", true));
            if(!ttsDir.exists()){
                ttsDir.create(true);
            }
            
            addParameter(inputText.set("Text", ""));
            addOutputParameter(outputPath.set("Out Path", ""));
            addParameter(playButton.set("Play"));
            addParameter(writeButton.set("Write"));
            addParameter(containerActive.set("Docker", false));
            addParameter(containerStatusColor.set("Status", ofColor(0)));
            addParameter(lastGeneratedFile.set("File", ""));
            addOutputParameter(trigger.set("Trigger", 0, 0, 1));
            
            if(!ofFile::doesFileExist(soxPath)) {
                ofLogError("Catotron") << "Sox not found at " << soxPath << ". Install with 'brew install sox'";
            }
            
            // Check for docker-compose.yml
            if(!ofFile::doesFileExist(dockerComposeDir + "/docker-compose.yml")) {
                ofLogError("Catotron") << "docker-compose.yml not found in: " << dockerComposeDir;
                containerStatusColor.set(ofColor(255, 0, 0));
            }
            else if(checkContainerStatus()) {
                ofLogNotice("Catotron") << "Found existing active container";
                containerStatus = true;
                containerActive = true;
                containerStatusColor.set(ofColor(0, 255, 0));
            }
            
            listeners.push(playButton.newListener([this](){
                if(containerStatus) executeTTSPlay();
                else ofLogError("Catotron") << "Docker container not active";
            }));
            
            listeners.push(writeButton.newListener([this](){
                if(containerStatus) executeTTSWrite();
                else ofLogError("Catotron") << "Docker container not active";
            }));
            
            listeners.push(containerActive.newListener([this](bool& active){
                ofLogNotice("Catotron") << "Container toggle: " << (active ? "ON" : "OFF");
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
                        ofLogNotice("Catotron") << "Write completed successfully";
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
    int executeCommand(const string& cmd) {
            string fullCmd = "cd \"" + dockerComposeDir + "\" && " + cmd;
            return system((fullCmd + " >/dev/null 2>&1").c_str());
        }
    
    void cleanupExistingContainers() {
        ofLogNotice("Catotron") << "Cleaning up existing containers...";
        
        // First try to stop and remove using docker compose
        string composeDown = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose down";
        system((composeDown + " 2>&1").c_str());
        
        // Additionally, try to force remove the container if it exists
        string forceRemove = dockerPath + " rm -f ttsapi";
        system((forceRemove + " 2>&1").c_str());
        
        // Give it a moment to clean up
        ofSleepMillis(1000);
    }

    void activateContainer() {
        ofLogNotice("Catotron") << "Activating container...";
        ofLogNotice("Catotron") << "Docker compose dir: " << dockerComposeDir;
        
        if(!ofFile::doesFileExist(dockerComposeDir + "/docker-compose.yml")) {
            ofLogError("Catotron") << "docker-compose.yml not found in: " << dockerComposeDir;
            containerActive = false;
            containerStatusColor.set(ofColor(255, 0, 0));
            return;
        }
        
        // Enhanced cleanup
        cleanupExistingContainers();
        
        // Start container using docker compose
        string cmd = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose up -d";
        ofLogNotice("Catotron") << "Start command: " << cmd;
        int result = system((cmd + " 2>&1").c_str());
        
        ofLogNotice("Catotron") << "Container start result: " << result;
        
        // Give it more time to start since we saw the build was successful but slower
        ofSleepMillis(5000); // Increased from 3000 to 5000
        
        containerStatus = checkContainerStatus();
        containerStatusColor.set(containerStatus ? ofColor(0, 255, 0) : ofColor(255, 0, 0));
        
        ofLogNotice("Catotron") << "Container status: " << (containerStatus ? "ACTIVE" : "FAILED");
        
        if(!containerStatus) {
            containerActive = false;
            ofLogError("Catotron") << "Failed to start Docker container";
        }
    }

    bool checkContainerStatus() {
        // First check if container exists and is running
        string checkCmd = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose ps | grep ttsapi";
        int result = system((checkCmd + " >/dev/null 2>&1").c_str());
        
        if(result != 0) {
            ofLogError("Catotron") << "Container not running";
            return false;
        }
        
        // Then check if service is responding
        ofSleepMillis(1000); // Give it a moment
        
        // Try multiple times to connect to the service
        for(int i = 0; i < 3; i++) {
            string curlCmd = "curl -s http://127.0.0.1:5050/api/short -o /dev/null";
            result = system((curlCmd + " >/dev/null 2>&1").c_str());
            
            if(result == 0) {
                ofLogNotice("Catotron") << "Service is responding";
                return true;
            }
            
            ofLogNotice("Catotron") << "Service not ready, attempt " << (i+1) << " of 3";
            ofSleepMillis(1000);
        }
        
        ofLogError("Catotron") << "Service failed to respond after 3 attempts";
        return false;
    }
    
    void deactivateContainer() {
        ofLogNotice("Catotron") << "Deactivating container...";
        cleanupExistingContainers();
        containerStatus = false;
        containerStatusColor.set(ofColor(0));
    }
    
    bool isPortAvailable() {
        string cmd = "lsof -i :5050";
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
    
    void executeTTSPlay() {
        if(inputText.get().empty()) {
            ofLogWarning("Catotron") << "No text specified";
            return;
        }
        
        ofLogNotice("Catotron") << "Executing TTS Play...";
        
        string tempFile = ofToDataPath("tts/temp_tts.wav", true);
        string tempFile441 = ofToDataPath("tts/temp_tts_441.wav", true);
        string tempJson = ofToDataPath("tts/temp.json", true);
        
        // Create JSON file with proper escaping
        string jsonContent = "{\"text\":\"" + inputText.get() + "\",\"lang\":\"ca\"}";
        
        ofFile jsonFile(tempJson, ofFile::WriteOnly);
        jsonFile.write(jsonContent.c_str(), jsonContent.length());
        jsonFile.close();
        
        string cmdCreate = "curl -X POST http://127.0.0.1:5050/api/short "
                         "-H \"Content-Type: application/json\" "
                         "-d @\"" + tempJson + "\" "
                         "> \"" + tempFile + "\" && "
                         + soxPath + " \"" + tempFile + "\" -r 44100 \"" + tempFile441 + "\" && "
                         "rm \"" + tempFile + "\" \"" + tempJson + "\"";
        
        ofLogNotice("Catotron") << "Executing curl command...";
        int result = system(cmdCreate.c_str());
        ofLogNotice("Catotron") << "Curl result: " << result;
        
        if(result == 0) {
            lastGeneratedFile.set(tempFile441);
            
            string cmdPlay = "afplay \"" + tempFile441 + "\"";
            system(cmdPlay.c_str());
            
            string cmdCleanup = "rm \"" + tempFile441 + "\"";
            system(cmdCleanup.c_str());
        } else {
            ofLogError("Catotron") << "Failed to generate audio";
        }
    }

    void executeTTSWrite() {
            if(inputText.get().empty()) {
                ofLogWarning("Catotron") << "No text specified";
                return;
            }
            
            if (writeInProgress) {
                ofLogWarning("Catotron") << "Write operation already in progress";
                return;
            }
            
            writeInProgress = true;
            writeFuture = std::async(std::launch::async, [this]() {
                ofLogNotice("Catotron") << "Executing TTS Write...";
                
                // Generate the filename using the current index
                std::stringstream ss;
                ss << std::setw(2) << std::setfill('0') << currentFileIndex;
                string indexStr = ss.str();
                
                string tempFile = ofToDataPath("tts/temp_tts.wav", true);
                string outputFile = ofToDataPath("tts/catotron_" + indexStr + ".wav", true);
                string tempJson = ofToDataPath("tts/temp.json", true);
                
                // Increment the file index for next time
                currentFileIndex = (currentFileIndex + 1) % maxFiles;
                
                string jsonContent = "{\"text\":\"" + inputText.get() + "\",\"lang\":\"ca\"}";
                
                ofFile jsonFile(tempJson, ofFile::WriteOnly);
                jsonFile.write(jsonContent.c_str(), jsonContent.length());
                jsonFile.close();
                
                string cmd = "curl -X POST http://127.0.0.1:5050/api/short "
                           "-H \"Content-Type: application/json\" "
                           "-d @\"" + tempJson + "\" "
                           "> \"" + tempFile + "\" && "
                           + soxPath + " \"" + tempFile + "\" -r 44100 \"" + outputFile + "\" && "
                           "rm \"" + tempFile + "\" \"" + tempJson + "\"";
                
                int result = system(cmd.c_str());
                
                if(result == 0) {
                    lastGeneratedFile.set(outputFile);
                    outputPath.set(outputFile);
                    ofLogNotice("Catotron") << "File saved: " + outputFile;
                    return true;
                } else {
                    ofLogError("Catotron") << "Failed to save file";
                    return false;
                }
            });
        }
    
    ofParameter<string> inputText;
        ofParameter<string> outputPath;
        ofParameter<void> playButton;
        ofParameter<void> writeButton;
        ofParameter<bool> containerActive;
        ofParameter<string> lastGeneratedFile;
        ofParameter<ofColor> containerStatusColor;
        ofParameter<int> trigger;
        
        bool containerStatus;
        bool writeInProgress;
        std::future<bool> writeFuture;
        
        string dockerPath;
        string soxPath;
        string dockerComposeDir;
        int triggerCounter;
        int triggerStartFrame;
        int currentFileIndex;
        int maxFiles;
        
        ofEventListeners listeners;
};

#endif /* Catotron_h */
