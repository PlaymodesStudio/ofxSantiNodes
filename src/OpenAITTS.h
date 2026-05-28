#ifndef OpenAITTS_h
#define OpenAITTS_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include <algorithm>
#include <future>
#include <thread>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

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
		description = "Text-to-Speech node using OpenAI's speech API. Uses gpt-4o-mini-tts by default with the latest OpenAI voices. Requires: 1) Miniconda installed via 'brew install --cask miniconda', 2) OpenAI Python package installed via '/opt/homebrew/Caskroom/miniconda/base/bin/pip install openai', 3) SoX audio utility installed via 'brew install sox', and 4) OPENAI_API_KEY set in the environment or stored in data/openai/openai_api_key.txt.";
        
		addParameterDropdown(selectedVoice, "Voice", getDefaultVoiceIndex(), getVoiceOptions());
		addParameter(instructionsText.set("Instructions", ""));
		addParameter(inputText.set("Text", ""));
        addInspectorParameter(editorHeight.set("Editor Height", 220.0f, 100.0f, 700.0f));

        textEditorRegion.set("Text Editor", [this](){
            drawTextEditor();
        });
        addCustomRegion(textEditorRegion, textEditorRegion.get());

		addParameter(writeButton.set("Write"));
		addParameter(lastGeneratedFile.set("File", ""));
		addOutputParameter(trigger.set("Trigger", 0, 0, 1));

        syncBufferFromParam(inputText.get());
        listeners.push(inputText.newListener([this](string &text){
            syncBufferFromParam(text);
        }));
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
    const vector<string>& getVoiceOptions() const {
        static const vector<string> voiceOptions = {
            "alloy", "ash", "ballad", "coral", "echo", "fable",
            "nova", "onyx", "sage", "shimmer", "verse", "marin", "cedar"
        };
        return voiceOptions;
    }

    int getDefaultVoiceIndex() const {
        const auto &voiceOptions = getVoiceOptions();
        auto it = std::find(voiceOptions.begin(), voiceOptions.end(), "marin");
        return it == voiceOptions.end() ? 0 : static_cast<int>(std::distance(voiceOptions.begin(), it));
    }

    string shellQuote(const string &value) const {
        string escaped = value;
        ofStringReplace(escaped, "'", "'\"'\"'");
        return "'" + escaped + "'";
    }

    string normalizedTextForTTS(const string &text) const {
        string normalized = text;
        ofStringReplace(normalized, "\r\n", "\n");
        ofStringReplace(normalized, "\r", "\n");
        ofStringReplace(normalized, "\n", " ");

        string compact;
        compact.reserve(normalized.size());
        bool previousWasWhitespace = false;
        for(char c : normalized) {
            bool isWhitespace = (c == ' ' || c == '\t');
            if(isWhitespace) {
                if(!previousWasWhitespace) {
                    compact += ' ';
                }
            } else {
                compact += c;
            }
            previousWasWhitespace = isWhitespace;
        }

        if(!compact.empty() && compact.front() == ' ') compact.erase(compact.begin());
        if(!compact.empty() && compact.back() == ' ') compact.pop_back();
        return compact;
    }

    void buildWrappedDisplayText(const string &text, float maxWidth, string &wrapped, std::vector<char> &autoFlags) const {
        wrapped.clear();
        autoFlags.clear();

        if(maxWidth <= 1.0f) {
            wrapped = text;
            autoFlags.assign(text.size(), 0);
            return;
        }

        auto appendChar = [&](char c, bool autoInserted) {
            wrapped += c;
            autoFlags.push_back(autoInserted ? 1 : 0);
        };

        auto wrapParagraph = [&](const string &paragraph) {
            if(paragraph.empty()) return;

            string paragraphWrapped;
            std::vector<char> paragraphFlags;
            string currentLine;
            string currentWord;

            auto pushLine = [&]() {
                if(currentLine.empty()) return;
                if(!paragraphWrapped.empty()) {
                    paragraphWrapped += "\n";
                    paragraphFlags.push_back(1);
                }
                paragraphWrapped += currentLine;
                paragraphFlags.insert(paragraphFlags.end(), currentLine.size(), 0);
                currentLine.clear();
            };

            auto appendWord = [&](const string &word) {
                if(word.empty()) return;

                string candidate = currentLine.empty() ? word : currentLine + " " + word;
                if(ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) {
                    currentLine = candidate;
                    return;
                }

                if(!currentLine.empty()) {
                    pushLine();
                }

                if(ImGui::CalcTextSize(word.c_str()).x <= maxWidth) {
                    currentLine = word;
                    return;
                }

                string chunk;
                for(char c : word) {
                    string nextChunk = chunk + c;
                    if(!chunk.empty() && ImGui::CalcTextSize(nextChunk.c_str()).x > maxWidth) {
                        if(!paragraphWrapped.empty()) {
                            paragraphWrapped += "\n";
                            paragraphFlags.push_back(1);
                        }
                        paragraphWrapped += chunk;
                        paragraphFlags.insert(paragraphFlags.end(), chunk.size(), 0);
                        chunk = string(1, c);
                    } else {
                        chunk = nextChunk;
                    }
                }
                currentLine = chunk;
            };

            for(char c : paragraph) {
                if(c == ' ' || c == '\t') {
                    appendWord(currentWord);
                    currentWord.clear();
                } else {
                    currentWord += c;
                }
            }

            appendWord(currentWord);
            pushLine();

            wrapped += paragraphWrapped;
            autoFlags.insert(autoFlags.end(), paragraphFlags.begin(), paragraphFlags.end());
        };

        string paragraph;
        for(char c : text) {
            if(c == '\n') {
                wrapParagraph(paragraph);
                paragraph.clear();
                appendChar('\n', false);
            } else {
                paragraph += c;
            }
        }
        wrapParagraph(paragraph);
    }

    void syncBufferFromParam(const string &text) {
        string wrapped;
        std::vector<char> flags;
        buildWrappedDisplayText(text, lastWrapWidth, wrapped, flags);
        inputTextBuffer.assign(wrapped.begin(), wrapped.end());
        inputTextBuffer.push_back('\0');
        autoWrapFlags = std::move(flags);
    }

    string rebuildRawTextFromEditedDisplay(const string &oldDisplay, const std::vector<char> &oldAutoFlags, const string &newDisplay) const {
        const int n = static_cast<int>(oldDisplay.size());
        const int m = static_cast<int>(newDisplay.size());
        std::vector<std::vector<int>> lcs(n + 1, std::vector<int>(m + 1, 0));

        for(int i = n - 1; i >= 0; --i) {
            for(int j = m - 1; j >= 0; --j) {
                if(oldDisplay[i] == newDisplay[j]) {
                    lcs[i][j] = lcs[i + 1][j + 1] + 1;
                } else {
                    lcs[i][j] = std::max(lcs[i + 1][j], lcs[i][j + 1]);
                }
            }
        }

        string raw;
        raw.reserve(newDisplay.size());
        int i = 0;
        int j = 0;
        while(i < n || j < m) {
            bool canMatch = i < n && j < m && oldDisplay[i] == newDisplay[j] &&
                            lcs[i][j] == lcs[i + 1][j + 1] + 1;
            if(canMatch) {
                if(!(oldAutoFlags[i] && oldDisplay[i] == '\n')) {
                    raw += oldDisplay[i];
                }
                ++i;
                ++j;
            } else if(j < m && (i == n || lcs[i][j + 1] >= lcs[i + 1][j])) {
                raw += newDisplay[j];
                ++j;
            } else if(i < n) {
                ++i;
            }
        }

        return raw;
    }

    void drawTextEditor() {
        const auto &customRegionContext = ofxOceanodeShared::getCustomRegionRenderContext();
        float zoom = ofxOceanodeShared::getZoomLevel();
        const float width = customRegionContext.active ? std::max(1.0f, customRegionContext.width) : 240.0f * zoom;
        const float height = customRegionContext.active ? std::max(1.0f, customRegionContext.height) : editorHeight.get() * zoom;
        const float padding = 6.0f * zoom;
        const float textWidth = width - padding * 2.0f - 12.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 22, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(18, 22, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(195, 220, 175, 255));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(10, 12, 18, 200));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(60, 80, 120, 200));

        ImGui::BeginChild("OpenAITTSTextEditor",
                          ImVec2(width, height + padding * 2.0f),
                          true,
                          ImGuiWindowFlags_None);
        ImGui::SetCursorPos(ImVec2(padding, padding));

        if(inputTextBuffer.empty()) {
            inputTextBuffer.push_back('\0');
        }

        if(std::abs(lastWrapWidth - textWidth) > 1.0f) {
            lastWrapWidth = textWidth;
            syncBufferFromParam(inputText.get());
        }

        string previousDisplay(inputTextBuffer.data());
        const bool changed = ImGui::InputTextMultiline(
            "##openai_tts_text",
            inputTextBuffer.data(),
            inputTextBuffer.size(),
            ImVec2(width - padding * 2.0f, height),
            ImGuiInputTextFlags_AllowTabInput |
            ImGuiInputTextFlags_CallbackResize,
            [](ImGuiInputTextCallbackData *data) -> int {
                if(data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                    auto *buffer = reinterpret_cast<std::vector<char>*>(data->UserData);
                    buffer->resize(data->BufTextLen + 1);
                    data->Buf = buffer->data();
                }
                return 0;
            },
            &inputTextBuffer
        );

        if(changed) {
            string editedDisplay(inputTextBuffer.data());
            string rawText = rebuildRawTextFromEditedDisplay(previousDisplay, autoWrapFlags, editedDisplay);
            inputText.set(rawText);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(5);
    }

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
                string ttsText = normalizedTextForTTS(inputText.get());

                const auto &voiceOptions = getVoiceOptions();
                int clampedVoiceIndex = ofClamp(selectedVoice.get(), 0, static_cast<int>(voiceOptions.size()) - 1);
                string selectedVoiceStr = voiceOptions[clampedVoiceIndex];

                string pythonCmd = "PYTHONPATH=\"" + pythonSitePackages + "\" " +
                                   "\"" + pythonBin + "\" \"" + pythonPath + "\"" +
                                   " --text " + shellQuote(ttsText) +
                                   " --output " + shellQuote(tempFile) +
                                   " --format wav" +
                                   " --voice " + shellQuote(selectedVoiceStr) +
                                   " --model gpt-4o-mini-tts";

                if(!instructionsText.get().empty()) {
                    pythonCmd += " --instructions " + shellQuote(instructionsText.get());
                }

                pythonCmd += " 2>&1";
                
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
    ofParameter<int> selectedVoice;
	ofParameter<string> instructionsText;
    ofParameter<float> editorHeight;
    customGuiRegion textEditorRegion;
    std::vector<char> inputTextBuffer;
    std::vector<char> autoWrapFlags;
    float lastWrapWidth = -1.0f;

        
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
