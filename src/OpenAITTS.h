#ifndef OpenAITTS_h
#define OpenAITTS_h

#include "ofxOceanodeNodeModel.h"
#include <future>
#include <thread>
#include <array>
#include <cstdio>

class OpenAITTS : public ofxOceanodeNodeModel {
public:
    OpenAITTS() : ofxOceanodeNodeModel("OpenAI TTS") {
        writeInProgress = false;
        triggerCounter = 0;
        pythonPath = ofToDataPath("openai/tts.py", true);
        soxPath = "/opt/homebrew/bin/sox";
        pythonBin = "/opt/homebrew/Caskroom/miniconda/base/bin/python3";
        pythonSitePackages = "/opt/homebrew/Caskroom/miniconda/base/lib/python3.12/site-packages";
        
        // Create tts output directory if it doesn't exist
        ofDirectory ttsDir(ofToDataPath("tts", true));
        if(!ttsDir.exists()){
            ttsDir.create(true);
        }
    }
    
    void setup() {
        description = "Text-to-Speech node using OpenAI's API. Generates natural sounding speech.";
        
        // Add voice selection dropdown
        vector<string> voiceOptions = {"alloy", "echo", "fable", "onyx", "nova", "shimmer"};
        addParameterDropdown(selectedVoice, "Voice", 4, voiceOptions); // Default to 'nova' (index 4)
        
        addParameter(inputText.set("Text", ""));
        addParameter(writeButton.set("Write"));
        addParameter(lastGeneratedFile.set("File", ""));
        addOutputParameter(trigger.set("Trigger", 0, 0, 1));
        
        listeners.push(writeButton.newListener([this](){
            executeTTSWrite();
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
                    ofLogNotice("OpenAITTS") << "Write completed successfully";
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
    string executeCommand(const string& cmd) {
            string result;
            FILE* pipe = popen(cmd.c_str(), "r");
            
            if (!pipe) {
                return "ERROR";
            }
            
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            
            pclose(pipe);
            return result;
        }

        string getFileType(const string& filePath) {
            string cmd = "file \"" + filePath + "\"";
            return executeCommand(cmd);
        }
    
    void executeTTSWrite() {
            if(inputText.get().empty()) {
                ofLogWarning("OpenAITTS") << "No text specified";
                return;
            }
            
            if (writeInProgress) {
                ofLogWarning("OpenAITTS") << "Write operation already in progress";
                return;
            }
            
            writeInProgress = true;
            writeFuture = std::async(std::launch::async, [this]() {
                ofLogNotice("OpenAITTS") << "Executing TTS Write...";
                
                string timestamp = ofGetTimestampString();
                string tempFile = ofToDataPath("tts/temp_tts.wav", true);
                string outputFile = ofToDataPath("tts/tts_" + timestamp + ".wav", true);
                
                string escapedText = inputText.get();
                ofStringReplace(escapedText, "'", "'\"'\"'");
                
                // Get the selected voice option
                vector<string> voiceOptions = {"alloy", "echo", "fable", "onyx", "nova", "shimmer"};
                string selectedVoiceStr = voiceOptions[selectedVoice];
                
                // Execute Python script with explicit response_format="wav" and selected voice
                string pythonCmd = "PYTHONPATH=\"" + pythonSitePackages + "\" " +
                                 "\"" + pythonBin + "\" \"" + pythonPath + "\" '" + escapedText + "' \"" + tempFile + "\" wav \"" + selectedVoiceStr + "\" 2>&1";
                
                ofLogNotice("OpenAITTS") << "Executing command: " << pythonCmd;
                string pythonOutput = executeCommand(pythonCmd);
                
                if(!pythonOutput.empty()) {
                    ofLogNotice("OpenAITTS") << "Python script output: " << pythonOutput;
                }
                
                if(!ofFile::doesFileExist(tempFile)) {
                    ofLogError("OpenAITTS") << "Python script failed to create temp file";
                    return false;
                }

                // Check file type
                string fileType = getFileType(tempFile);
                ofLogNotice("OpenAITTS") << "Generated file type: " << fileType;
                
                // Check if it's already a WAV file
                bool isWav = fileType.find("WAVE audio") != string::npos;
                
                if(!isWav) {
                    // If not WAV, use sox to convert (handles MP3 to WAV conversion)
                    string tempWav = tempFile + "_converted.wav";
                    string convertCmd = "\"" + soxPath + "\" \"" + tempFile + "\" \"" + tempWav + "\" 2>&1";
                    string convertOutput = executeCommand(convertCmd);
                    
                    if(!convertOutput.empty()) {
                        ofLogNotice("OpenAITTS") << "Convert output: " << convertOutput;
                    }
                    
                    // Remove original temp file
                    ofFile::removeFile(tempFile);
                    tempFile = tempWav;
                }
                
                // Now resample to 44.1kHz
                string resampleCmd = "\"" + soxPath + "\" \"" + tempFile + "\" -r 44100 \"" + outputFile + "\" 2>&1";
                string soxOutput = executeCommand(resampleCmd);
                
                if(!soxOutput.empty()) {
                    ofLogNotice("OpenAITTS") << "Sox output: " << soxOutput;
                }
                
                // Clean up temp files
                if(ofFile::doesFileExist(tempFile)) {
                    ofFile::removeFile(tempFile);
                }
                
                if(ofFile::doesFileExist(outputFile)) {
                    lastGeneratedFile.set(outputFile);
                    ofLogNotice("OpenAITTS") << "File saved: " + outputFile;
                    return true;
                }
                
                ofLogError("OpenAITTS") << "Failed to generate final audio file";
                return false;
            });
        }

    ofParameter<string> inputText;
    ofParameter<void> writeButton;
    ofParameter<string> lastGeneratedFile;
    ofParameter<int> trigger;
    ofParameter<int> selectedVoice;  // New parameter for voice selection
        
    bool writeInProgress;
    std::future<bool> writeFuture;
    string pythonPath;
    string soxPath;
    string pythonBin;
    string pythonSitePackages;
    int triggerCounter;
        
    ofEventListeners listeners;
};

#endif /* OpenAITTS_h */
