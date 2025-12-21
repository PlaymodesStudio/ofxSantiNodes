#ifndef reaperOscTransport_h
#define reaperOscTransport_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"

class reaperOscTransport : public ofxOceanodeNodeModel {
public:
	reaperOscTransport() : ofxOceanodeNodeModel("Reaper OSC Transport") {};
	
	~reaperOscTransport() {
		if(oscReceiver.isListening()) {
			oscReceiver.stop();
		}
	}
	
	void setup() {
		// Input parameters
		addParameter(port.set("Port", 9999, 1024, 65535));
		addParameter(enable.set("Enable", false));
		
		// Transport state outputs
		addOutputParameter(playState.set("Play", false));
		addOutputParameter(stopState.set("Stop", false));
		addOutputParameter(recordState.set("Record", false));
		addOutputParameter(pauseState.set("Pause", false));
		addOutputParameter(repeatState.set("Repeat", false));
		
		// Time/Position outputs
		addOutputParameter(timeSeconds.set("Time (s)", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(beat.set("Beat", 0.0f, 0.0f, FLT_MAX));
		addOutputParameter(beatStr.set("Beat Str", ""));
		addOutputParameter(samples.set("Samples", 0, 0, INT_MAX));
		addOutputParameter(frames.set("Frames", 0, 0, INT_MAX));
		addOutputParameter(ppq96.set("PPQ 96", 0, 0, INT_MAX));
		
		// Tempo output
		addOutputParameter(tempo.set("BPM", 120.0f, 1.0f, 999.0f));
		
		// Other outputs
		addOutputParameter(timeStr.set("Time Str", ""));
		addOutputParameter(timeSignature.set("Time Sig", "4/4"));
		
		// Marker output
		addOutputParameter(marker.set("Marker", 0, 0, INT_MAX));
		addOutputParameter(markerName.set("Marker Name", ""));
		
		// Listeners
		portListener = port.newListener([this](int &p) {
			if(enable) {
				restartOsc();
			}
		});
		
		enableListener = enable.newListener([this](bool &e) {
			if(e) {
				startOsc();
			} else {
				stopOsc();
			}
		});
	}
	
	void update(ofEventArgs &args) {
		if(!enable || !oscReceiver.isListening()) return;
		
		// Process all available OSC messages
		while(oscReceiver.hasWaitingMessages()) {
			ofxOscMessage msg;
			oscReceiver.getNextMessage(msg);
			
			string address = msg.getAddress();
			
			// Transport states - Reaper sends as floats (0.0 or 1.0)
			if(address == "/play") {
				if(msg.getNumArgs() > 0) {
					playState = (msg.getArgAsFloat(0) > 0.5f);
				}
			}
			else if(address == "/stop") {
				if(msg.getNumArgs() > 0) {
					stopState = (msg.getArgAsFloat(0) > 0.5f);
				}
			}
			else if(address == "/record") {
				if(msg.getNumArgs() > 0) {
					recordState = (msg.getArgAsFloat(0) > 0.5f);
				}
			}
			else if(address == "/pause") {
				if(msg.getNumArgs() > 0) {
					pauseState = (msg.getArgAsFloat(0) > 0.5f);
				}
			}
			else if(address == "/repeat") {
				if(msg.getNumArgs() > 0) {
					repeatState = (msg.getArgAsFloat(0) > 0.5f);
				}
			}
			// Time/Position
			else if(address == "/time") {
				if(msg.getNumArgs() > 0) {
					timeSeconds = msg.getArgAsFloat(0);
				}
			}
			else if(address == "/beat/str") {
				if(msg.getNumArgs() > 0) {
					string beatString = msg.getArgAsString(0);
					beatStr = beatString;
					
					// FIXED: Parse beat string correctly
					// Format: "measure.beat.hundredths"
					// "1.1.11" = measure 1, beat 1, 11/100 (0.11) through the beat
					// "1.2.50" = measure 1, beat 2, 50/100 (0.50) through the beat
					vector<string> parts = ofSplitString(beatString, ".");
					if(parts.size() >= 2) {
						int measure = ofToInt(parts[0]);
						int beatInMeasure = ofToInt(parts[1]);
						float fraction = 0.0f;
						
						if(parts.size() >= 3) {
							// CRITICAL FIX: Third part is hundredths (0-99)
							// Convert to decimal fraction (0.00 - 0.99)
							fraction = ofToFloat(parts[2]) / 100.0f;
						}
						
						// Calculate total beats (assuming 4/4 time signature)
						// Measure 1, beat 1, 0% = 0.00
						// Measure 1, beat 1, 50% = 0.50
						// Measure 1, beat 2, 0% = 1.00
						// Measure 2, beat 1, 0% = 4.00
						float totalBeats = ((measure - 1) * 4.0f) + (beatInMeasure - 1) + fraction;
						
						beat = totalBeats;
						ppq96 = (int)(totalBeats * 96.0f);
					}
				}
			}
			else if(address == "/samples") {
				if(msg.getNumArgs() > 0) {
					if(msg.getArgType(0) == OFXOSC_TYPE_INT32) {
						samples = msg.getArgAsInt(0);
					} else if(msg.getArgType(0) == OFXOSC_TYPE_FLOAT) {
						samples = (int)msg.getArgAsFloat(0);
					} else if(msg.getArgType(0) == OFXOSC_TYPE_STRING) {
						samples = ofToInt(msg.getArgAsString(0));
					}
				}
			}
			// Tempo
			else if(address == "/tempo/raw") {
				if(msg.getNumArgs() > 0) {
					tempo = msg.getArgAsFloat(0);
				}
			}
			else if(address == "/tempo/str") {
				if(msg.getNumArgs() > 0) {
					// Parse string format - usually like "120.00" or "120.00 BPM"
					string tempoStr = msg.getArgAsString(0);
					ofStringReplace(tempoStr, " BPM", "");
					ofStringReplace(tempoStr, "BPM", "");
					tempo = ofToFloat(tempoStr);
				}
			}
			// Other
			else if(address == "/time/str") {
				if(msg.getNumArgs() > 0) {
					timeStr = msg.getArgAsString(0);
				}
			}
			// Marker
			else if(address == "/lastmarker/number/str") {
				if(msg.getNumArgs() > 0) {
					string markerNumStr = msg.getArgAsString(0);
					marker = ofToInt(markerNumStr);
				}
			}
			else if(address == "/lastmarker/name") {
				if(msg.getNumArgs() > 0) {
					markerName = msg.getArgAsString(0);
				}
			}
		}
	}
	
private:
	void startOsc() {
		if(!oscReceiver.isListening()) {
			oscReceiver.setup(port);
		}
	}
	
	void stopOsc() {
		if(oscReceiver.isListening()) {
			oscReceiver.stop();
		}
	}
	
	void restartOsc() {
		stopOsc();
		startOsc();
	}
	
	ofxOscReceiver oscReceiver;
	
	// Parameters
	ofParameter<int> port;
	ofParameter<bool> enable;
	
	// Transport outputs
	ofParameter<bool> playState;
	ofParameter<bool> stopState;
	ofParameter<bool> recordState;
	ofParameter<bool> pauseState;
	ofParameter<bool> repeatState;
	
	// Time/Position outputs
	ofParameter<float> timeSeconds;
	ofParameter<float> beat;
	ofParameter<string> beatStr;
	ofParameter<int> samples;
	ofParameter<int> frames;
	ofParameter<int> ppq96;
	
	// Tempo output
	ofParameter<float> tempo;
	
	// Other outputs
	ofParameter<string> timeStr;
	ofParameter<string> timeSignature;
	
	// Marker outputs
	ofParameter<int> marker;
	ofParameter<string> markerName;
	
	// Listeners
	ofEventListener portListener;
	ofEventListener enableListener;
};

#endif /* reaperOscTransport_h */
