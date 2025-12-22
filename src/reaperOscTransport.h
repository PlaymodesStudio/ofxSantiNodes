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
		addSeparator("INPUTS", ofColor(240, 240, 240));
		addParameter(port.set("Port", 9999, 1024, 65535));
		addParameter(enable.set("Enable", false));
		addParameter(sampleRate.set("Sample Rate", 48000, 44100, 192000));
		
		/* -------- Transport outputs (matching MIDI Clock Transport) -------- */
		addSeparator("TRANSPORT", ofColor(240, 240, 240));
		
		addOutputParameter(playState.set("Play", false));
		playState.setSerializable(false);
		
		addOutputParameter(stopState.set("Stop", false));
		stopState.setSerializable(false);
		
		addOutputParameter(jumpTrig.set("Jump", false));
		jumpTrig.setSerializable(false);
		
		/* -------- Clock outputs (matching MIDI Clock Transport) -------- */
		addSeparator("CLOCK OUTPUTS", ofColor(240, 240, 240));
		
		addOutputParameter(beat.set("Beat", 0.f, 0.f, FLT_MAX));
		beat.setSerializable(false);
		
		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		ppq24.setSerializable(false);
		
		addOutputParameter(ppq24f.set("PPQ 24f", 0.f, 0.f, FLT_MAX));
		ppq24f.setSerializable(false);
		
		addOutputParameter(timeSeconds.set("Time(s)", 0.f, 0.f, FLT_MAX));
		timeSeconds.setSerializable(false);
		
		addOutputParameter(bpm.set("BPM", 120.f, 1.f, 999.f));
		
		/* -------- Additional Reaper-specific outputs -------- */
		addSeparator("AUX OUTPUTS", ofColor(240, 240, 240));
		
		addOutputParameter(recordState.set("Record", false));
		recordState.setSerializable(false);
		
		addOutputParameter(pauseState.set("Pause", false));
		pauseState.setSerializable(false);
		
		addOutputParameter(repeatState.set("Repeat", false));
		repeatState.setSerializable(false);
		
		addOutputParameter(beatStr.set("Beat Str", ""));
		beatStr.setSerializable(false);
		
		addOutputParameter(timeStr.set("Time Str", ""));
		timeStr.setSerializable(false);
		
		addOutputParameter(timeSignature.set("Time Sig", "4/4"));
		
		addOutputParameter(samples.set("Samples", 0, 0, INT_MAX));
		samples.setSerializable(false);
		
		addOutputParameter(marker.set("Marker", 0, 0, INT_MAX));
		marker.setSerializable(false);
		
		addOutputParameter(markerName.set("Marker Name", ""));
		markerName.setSerializable(false);
		
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
		
		resetTransport();
	}
	
	void update(ofEventArgs &args) {
		if(!enable || !oscReceiver.isListening()) return;
		
		// Reset jump trigger at start of frame
		bool hadJump = false;
		
		// Track if we received position update this frame
		bool receivedPositionUpdate = false;
		float newTimeSeconds = timeSeconds.get();
		
		// Process all available OSC messages
		while(oscReceiver.hasWaitingMessages()) {
			ofxOscMessage msg;
			oscReceiver.getNextMessage(msg);
			
			string address = msg.getAddress();
			
			// Transport states
			if(address == "/play") {
				if(msg.getNumArgs() > 0) {
					bool newPlayState = (msg.getArgAsFloat(0) > 0.5f);
					
					// Detect play start
					if(newPlayState && !playState.get()) {
						hadJump = true;
						lastUpdateTime = ofGetElapsedTimef();
						ofLogNotice("reaperOscTransport") << "Play started";
					}
					
					playState = newPlayState;
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
			
			// PREFERRED: High-precision time position
			else if(address == "/time") {
				if(msg.getNumArgs() > 0) {
					newTimeSeconds = msg.getArgAsFloat(0);
					receivedPositionUpdate = true;
				}
			}
			
			// ALTERNATIVE: Sample-accurate position
			else if(address == "/samples") {
				if(msg.getNumArgs() > 0) {
					int samplePos = 0;
					if(msg.getArgType(0) == OFXOSC_TYPE_INT32) {
						samplePos = msg.getArgAsInt(0);
					} else if(msg.getArgType(0) == OFXOSC_TYPE_FLOAT) {
						samplePos = (int)msg.getArgAsFloat(0);
					} else if(msg.getArgType(0) == OFXOSC_TYPE_STRING) {
						samplePos = ofToInt(msg.getArgAsString(0));
					}
					
					samples = samplePos;
					
					// Convert samples to seconds (use this if /time not available)
					if(sampleRate.get() > 0 && !receivedPositionUpdate) {
						newTimeSeconds = (float)samplePos / (float)sampleRate.get();
						receivedPositionUpdate = true;
					}
				}
			}
			
			// FALLBACK: Beat string (less precise)
			else if(address == "/beat/str") {
				if(msg.getNumArgs() > 0) {
					beatStr = msg.getArgAsString(0);
				}
			}
			
			// Tempo
			else if(address == "/tempo/raw") {
				if(msg.getNumArgs() > 0) {
					bpm = msg.getArgAsFloat(0);
				}
			}
			else if(address == "/tempo/str") {
				if(msg.getNumArgs() > 0) {
					string tempoStr = msg.getArgAsString(0);
					ofStringReplace(tempoStr, " BPM", "");
					ofStringReplace(tempoStr, "BPM", "");
					float parsedBpm = ofToFloat(tempoStr);
					// Only use string if we don't have raw
					if(bpm.get() == 120.f || parsedBpm != 120.f) {
						bpm = parsedBpm;
					}
				}
			}
			
			// Time signature (not sent by default Reaper OSC)
			else if(address == "/time/signature") {
				if(msg.getNumArgs() > 0) {
					timeSignature = msg.getArgAsString(0);
				}
			}
			
			// Time string
			else if(address == "/time/str") {
				if(msg.getNumArgs() > 0) {
					timeStr = msg.getArgAsString(0);
				}
			}
			
			// Marker
			else if(address == "/lastmarker/number/str") {
				if(msg.getNumArgs() > 0) {
					marker = ofToInt(msg.getArgAsString(0));
				}
			}
			else if(address == "/lastmarker/name") {
				if(msg.getNumArgs() > 0) {
					markerName = msg.getArgAsString(0);
				}
			}
		}
		
		// Process position update if received
		if(receivedPositionUpdate) {
			// Detect jumps
			if(lastTimeSeconds >= 0) {
				float timeDelta = newTimeSeconds - lastTimeSeconds;
				
				// Backwards or large forward jump
				if(timeDelta < -0.001f || timeDelta > 0.1f) {
					hadJump = true;
					ofLogNotice("reaperOscTransport")
						<< "Jump detected: " << lastTimeSeconds
						<< "s -> " << newTimeSeconds << "s"
						<< " (delta: " << timeDelta << "s)";
				}
			}
			
			lastTimeSeconds = newTimeSeconds;
			currentTimeSeconds = newTimeSeconds;
			timeSeconds = newTimeSeconds;
			lastUpdateTime = ofGetElapsedTimef();
		}
		
		// Interpolate during playback for smooth output
		if(playState.get() && !pauseState.get()) {
			float now = ofGetElapsedTimef();
			float dt = now - lastUpdateTime;
			
			// Only interpolate if we have a reasonable delta time and didn't just receive an update
			if(dt > 0 && dt < 1.0f && !receivedPositionUpdate) {
				currentTimeSeconds += dt;
				lastUpdateTime = now;
			}
		}
		
		// Update time output
		timeSeconds = currentTimeSeconds;
		
		// Calculate beat and PPQ from time and BPM
		if(bpm.get() > 0) {
			// beats = (seconds * BPM) / 60
			float currentBeat = (currentTimeSeconds * bpm.get()) / 60.0f;
			beat = currentBeat;
			
			// High-precision floating-point PPQ
			float ppqFloat = currentBeat * 24.0f;
			ppq24f = ppqFloat;
			
			// Integer PPQ for compatibility
			ppq24 = (int)ppqFloat;
		}
		
		jumpTrig = hadJump;
	}
	
private:
	void startOsc() {
		if(!oscReceiver.isListening()) {
			oscReceiver.setup(port);
			ofLogNotice("reaperOscTransport")
				<< "Listening on port: " << port.get();
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
	
	void resetTransport() {
		playState = false;
		stopState = false;
		jumpTrig = false;
		beat = 0.f;
		ppq24 = 0;
		ppq24f = 0.f;
		timeSeconds = 0.f;
		bpm = 120.f;
		lastTimeSeconds = -1.f;
		currentTimeSeconds = 0.f;
		lastUpdateTime = 0.f;
	}
	
	ofxOscReceiver oscReceiver;
	
	// State tracking for interpolation
	float lastTimeSeconds = -1.f;
	float currentTimeSeconds = 0.f;
	float lastUpdateTime = 0.f;
	
	// Parameters
	ofParameter<int> port;
	ofParameter<bool> enable;
	ofParameter<int> sampleRate;
	
	/* -------- Core transport outputs -------- */
	ofParameter<bool> playState;
	ofParameter<bool> stopState;
	ofParameter<bool> jumpTrig;
	
	ofParameter<float> beat;
	ofParameter<int> ppq24;
	ofParameter<float> ppq24f;
	ofParameter<float> timeSeconds;
	ofParameter<float> bpm;
	
	/* -------- Additional outputs -------- */
	ofParameter<bool> recordState;
	ofParameter<bool> pauseState;
	ofParameter<bool> repeatState;
	
	ofParameter<string> beatStr;
	ofParameter<string> timeStr;
	ofParameter<string> timeSignature;
	
	ofParameter<int> samples;
	ofParameter<int> marker;
	ofParameter<string> markerName;
	
	// Listeners
	ofEventListener portListener;
	ofEventListener enableListener;
};

#endif /* reaperOscTransport_h */
