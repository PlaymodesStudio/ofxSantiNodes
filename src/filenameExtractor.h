#ifndef filenameExtractor_h
#define filenameExtractor_h

#include "ofxOceanodeNodeModel.h"

class filenameExtractor : public ofxOceanodeNodeModel {
public:
	filenameExtractor() : ofxOceanodeNodeModel("Filename Extractor") {}

	void setup() override {
		description = "Extracts filename without extension from a full path.";
		
		addParameter(inputPath.set("Path", ""));
		addOutputParameter(outputFilename.set("Filename", ""));
		
		listener = inputPath.newListener([this](string &path){
			extractFilename(path);
		});
	}

private:
	void extractFilename(const string &path) {
		if(path.empty()) {
			outputFilename = "";
			return;
		}

		// Find last slash or backslash
		size_t lastSlash = path.find_last_of("/\\");
		if(lastSlash == string::npos) {
			lastSlash = 0;
		} else {
			lastSlash += 1;
		}

		// Find the last dot after the last slash
		size_t lastDot = path.find_last_of(".");
		if(lastDot == string::npos || lastDot < lastSlash) {
			lastDot = path.length();
		}

		// Extract the filename portion
		string filename = path.substr(lastSlash, lastDot - lastSlash);
		outputFilename = filename;
	}

	ofParameter<string> inputPath;
	ofParameter<string> outputFilename;
	ofEventListener listener;
};

#endif /* filenameExtractor_h */
