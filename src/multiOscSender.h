#ifndef multiOscSender_h
#define multiOscSender_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"

class multiOscSender : public ofxOceanodeNodeModel {
public:
	multiOscSender() : ofxOceanodeNodeModel("FORMS->P5 OSC Sender") {
		
		// Initialize new integer parameters
		barsInput.set("/bars", 4, 0, INT_MAX);
		notegridInput.set("/notegrid", 100, 0, INT_MAX);
		paletaInput.set("/paleta", 3, 0, INT_MAX);
		escalaInput.set("/escala", 4, 0, INT_MAX);
		snapshotInput.set("/snapshot", -1, -1, INT_MAX);
		bpmInput.set("/bpm", 120, 1, 999);
		
		// Initialize vectors with their actual sizes and ranges
		vector<int> transposeDefault(4, 0);
		vector<int> transposeMin(4, INT_MIN);
		vector<int> transposeMax(4, INT_MAX);
		transposeList.set("/transposeList", transposeDefault, transposeMin, transposeMax);
		
		vector<int> rootDefault(5, 0);
		vector<int> rootMin(5, INT_MIN);
		vector<int> rootMax(5, INT_MAX);
		rootList.set("/rootList", rootDefault, rootMin, rootMax);
		
		// Initialize string parameters
		layer3Input.set("/layer3", "");
		fx3Input.set("/fx3", "");
		layer2Input.set("/layer2", "");
		fx2Input.set("/fx2", "");
		layer1Input.set("/layer1", "");
		fx1Input.set("/fx1", "");
		
		goButton.set("/go", false);
		
		oscPort.set("Port", 12346, 0, 65535);
		oscHost.set("Host", "127.0.0.1");
	}
	
	void setup() {
		addParameter(barsInput);
		addParameter(notegridInput);
		addParameter(paletaInput);
		addParameter(escalaInput);
		addParameter(snapshotInput);
		addParameter(bpmInput); // Add the BPM parameter to the node
		
		addParameter(transposeList);
		addParameter(rootList);
		
		addParameter(layer3Input);
		addParameter(fx3Input);
		addParameter(layer2Input);
		addParameter(fx2Input);
		addParameter(layer1Input);
		addParameter(fx1Input);
	
		addParameter(goButton);
		addParameter(oscPort);
		addParameter(oscHost);
		
		listeners.push(oscHost.newListener([this](string &s){
			sender.setup(oscHost, oscPort);
		}));
		
		listeners.push(oscPort.newListener([this](int &p){
			sender.setup(oscHost, oscPort);
		}));
		
		listeners.push(goButton.newListener([this](bool &b){
			if(b) sendAllMessages();
		}));
		
		sender.setup(oscHost, oscPort);
	}
	
private:
	void sendStringMessage(const string& address, const string& input) {
		if(!sender.isReady()) return;
		
		ofxOscMessage message;
		message.setAddress(address);
		
		if(input.empty()) {
			// For empty input, send count 1 and "none"
			message.addIntArg(1);
			message.addStringArg("none");
		} else {
			// Split string and keep all non-empty parts
			vector<string> splitStrings = ofSplitString(input, " ");
			vector<string> nonEmptyStrings;
			
			for(const auto& str : splitStrings) {
				if(!str.empty()) {
					nonEmptyStrings.push_back(str);
				}
			}
			
			message.addIntArg(nonEmptyStrings.size());
			for(const auto& str : nonEmptyStrings) {
				message.addStringArg(str);
			}
		}
		
		sender.sendMessage(message);
	}
	
	void sendIntListMessage(const string& address, const vector<int>& list) {
		if(!sender.isReady()) return;
		
		ofxOscMessage message;
		message.setAddress(address);
		message.addIntArg(list.size());
		for(int val : list) {
			message.addIntArg(val);
		}
		sender.sendMessage(message);
	}
	
	void sendSingleIntMessage(const string& address, int value) {
		if(!sender.isReady()) return;
		
		ofxOscMessage message;
		message.setAddress(address);
		message.addIntArg(value);
		sender.sendMessage(message);
	}
	
	void sendAllMessages() {
		// Send the new single int messages
		sendSingleIntMessage("/bars", barsInput);
		sendSingleIntMessage("/notegrid", notegridInput);
		sendSingleIntMessage("/paleta", paletaInput);
		sendSingleIntMessage("/escala", escalaInput);
		sendSingleIntMessage("/snapshot", snapshotInput);
		sendSingleIntMessage("/bpm", bpmInput); // Send the BPM message
		
		// Send int list messages only if they contain non-zero values
		sendIntListMessage("/transposeList", transposeList);
		sendIntListMessage("/rootList", rootList);
		
		// Always send all string messages
		sendStringMessage("/layer3", layer3Input);
		sendStringMessage("/fx3", fx3Input);
		sendStringMessage("/layer2", layer2Input);
		sendStringMessage("/fx2", fx2Input);
		sendStringMessage("/layer1", layer1Input);
		sendStringMessage("/fx1", fx1Input);
	
		sendGoMessage();
		goButton = false;
	}
	
	void sendGoMessage() {
		if(!sender.isReady()) return;
		
		ofxOscMessage message;
		message.setAddress("/go");
		message.addIntArg(1);
		sender.sendMessage(message);
	}
	
	ofxOscSender sender;
	
	ofParameter<string> layer3Input;
	ofParameter<string> fx3Input;
	ofParameter<string> layer2Input;
	ofParameter<string> fx2Input;
	ofParameter<string> layer1Input;
	ofParameter<string> fx1Input;
	ofParameter<vector<int>> transposeList;
	ofParameter<vector<int>> rootList;
	
	// Integer parameters
	ofParameter<int> barsInput;
	ofParameter<int> notegridInput;
	ofParameter<int> paletaInput;
	ofParameter<int> escalaInput;
	ofParameter<int> snapshotInput;
	ofParameter<int> bpmInput; // Added BPM parameter declaration
	
	ofParameter<bool> goButton;
	ofParameter<int> oscPort;
	ofParameter<string> oscHost;
	
	ofEventListeners listeners;
};

#endif /* multiOscSender_h */
