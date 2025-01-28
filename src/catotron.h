#ifndef Catotron_h
#define Catotron_h

#include "ofxOceanodeNodeModel.h"
#include <future>
#include <thread>
#include <iomanip>
#include <sstream>
#include <curl/curl.h>

class Catotron : public ofxOceanodeNodeModel {
public:
    Catotron() : ofxOceanodeNodeModel("Catotron TTS") {
        containerStatus = false;
        dockerPath = "/usr/local/bin/docker";
        triggerCounter = 0;
        triggerStartFrame = 0;
        writeInProgress = false;
        currentFileIndex = 0;
        maxFiles = 20;
        
        // Initialize curl globally
        curl_global_init(CURL_GLOBAL_ALL);
        
        // Set docker compose directory to data/catotron
        dockerComposeDir = ofToDataPath("catotron", true);
    }
    
    ~Catotron() {
        // Cleanup curl
        curl_global_cleanup();
    }
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    static size_t WriteToFile(void* ptr, size_t size, size_t nmemb, void* stream) {
        size_t written = fwrite(ptr, size, nmemb, (FILE*)stream);
        return written;
    }
    
    bool performCurlRequest(const string& url, const string& jsonContent, const string& outputFile) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            ofLogError("Catotron") << "Failed to initialize curl";
            return false;
        }
        
        FILE* fp = fopen(outputFile.c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            ofLogError("Catotron") << "Failed to open output file";
            return false;
        }
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonContent.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(fp);
        
        if (res != CURLE_OK) {
            ofLogError("Catotron") << "Curl request failed: " << curl_easy_strerror(res);
            return false;
        }
        
        return true;
    }
    
    bool checkServiceAvailability() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return false;
        }
        
        string response;
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5050/api/short");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        return (res == CURLE_OK);
    }
    
    void setup() {
        description = "Catalan Text-to-Speech node that generates natural sounding speech using Catotron. Requires catotron-api Docker container.";
        
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
        
        string composeDown = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose down";
        system((composeDown + " 2>&1").c_str());
        
        string forceRemove = dockerPath + " rm -f ttsapi";
        system((forceRemove + " 2>&1").c_str());
        
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
        
        cleanupExistingContainers();
        
        string cmd = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose up -d";
        ofLogNotice("Catotron") << "Start command: " << cmd;
        int result = system((cmd + " 2>&1").c_str());
        
        ofLogNotice("Catotron") << "Container start result: " << result;
        
        ofSleepMillis(5000);
        
        containerStatus = checkContainerStatus();
        containerStatusColor.set(containerStatus ? ofColor(0, 255, 0) : ofColor(255, 0, 0));
        
        ofLogNotice("Catotron") << "Container status: " << (containerStatus ? "ACTIVE" : "FAILED");
        
        if(!containerStatus) {
            containerActive = false;
            ofLogError("Catotron") << "Failed to start Docker container";
        }
    }

    bool checkContainerStatus() {
        string checkCmd = "cd \"" + dockerComposeDir + "\" && " + dockerPath + " compose ps | grep ttsapi";
        int result = system((checkCmd + " >/dev/null 2>&1").c_str());
        
        if(result != 0) {
            ofLogError("Catotron") << "Container not running";
            return false;
        }
        
        ofSleepMillis(1000);
        
        for(int i = 0; i < 3; i++) {
            if(checkServiceAvailability()) {
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
        return result.empty();
    }
    
    void executeTTSPlay() {
        if(inputText.get().empty()) {
            ofLogWarning("Catotron") << "No text specified";
            return;
        }
        
        ofLogNotice("Catotron") << "Executing TTS Play...";
        
        string tempFile = ofToDataPath("tts/temp_tts.wav", true);
        string jsonContent = "{\"text\":\"" + inputText.get() + "\",\"lang\":\"ca\"}";
        
        if(performCurlRequest("http://127.0.0.1:5050/api/short", jsonContent, tempFile)) {
            lastGeneratedFile.set(tempFile);
            
            string playCmd = "afplay \"" + tempFile + "\"";
            system(playCmd.c_str());
            
            ofFile::removeFile(tempFile);
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
            
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << currentFileIndex;
            string indexStr = ss.str();
            
            string outputFile = ofToDataPath("tts/catotron_" + indexStr + ".wav", true);
            currentFileIndex = (currentFileIndex + 1) % maxFiles;
            
            string jsonContent = "{\"text\":\"" + inputText.get() + "\",\"lang\":\"ca\"}";
            
            if(performCurlRequest("http://127.0.0.1:5050/api/short", jsonContent, outputFile)) {
                lastGeneratedFile.set(outputFile);
                outputPath.set(outputFile);
                ofLogNotice("Catotron") << "File saved: " + outputFile;
                return true;
            }
            
            ofLogError("Catotron") << "Failed to save file";
            return false;
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
    string dockerComposeDir;
    int triggerCounter;
    int triggerStartFrame;
    int currentFileIndex;
    int maxFiles;
    
    ofEventListeners listeners;
};

#endif /* Catotron_h */
