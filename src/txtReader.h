#ifndef txtReader_h
#define txtReader_h

#include "ofxOceanodeNodeModel.h"
#include "ofFileUtils.h"

class txtReader : public ofxOceanodeNodeModel {
public:
    txtReader() : ofxOceanodeNodeModel("Text Reader") {}
    
    void setup() override {
        description = "Reads text from a file specified by path or selected through a file dialog. Line breaks are replaced with spaces.";
        
        // Add input parameter for file path
        addParameter(filePath.set("File Path", ""));
        
        // Add button to open file dialog
        addParameter(openFile.set("Open File"));
        
        // Add output parameter for text content
        addParameter(output.set("Output", ""));
        
        // Add output parameter to show if file exists
        addParameter(fileExists.set("File Exists", false));
        
        // Add listener for file path changes
        pathListener = filePath.newListener([this](string &path){
            readFile(path);
        });
        
        // Add listener for open file button
        openListener = openFile.newListener([this](){
            openFileDialog();
        });
    }
    
private:
    void readFile(const string &path) {
        if(path.empty()) {
            fileExists = false;
            output = "";
            return;
        }
        
        ofFile file(path);
        if(file.exists()) {
            fileExists = true;
            string content = ofBufferFromFile(path).getText();
            
            // Replace all Windows-style line breaks (\r\n) with spaces
            ofStringReplace(content, "\r\n", " ");
            // Replace all Unix-style line breaks (\n) with spaces
            ofStringReplace(content, "\n", " ");
            // Replace multiple consecutive spaces with a single space
            while(content.find("  ") != string::npos) {
                ofStringReplace(content, "  ", " ");
            }
            // Trim leading and trailing spaces
            content = ofTrim(content);
            
            output = content;
        } else {
            fileExists = false;
            output = "";
        }
    }
    
    void openFileDialog() {
        ofFileDialogResult result = ofSystemLoadDialog("Select a text file", false, "");
        
        if(result.bSuccess) {
            filePath = result.getPath();
            // readFile will be called automatically through the path listener
        }
    }
    
    ofParameter<string> filePath;
    ofParameter<void> openFile;
    ofParameter<string> output;
    ofParameter<bool> fileExists;
    ofEventListener pathListener;
    ofEventListener openListener;
};

#endif /* txtReader_h */
