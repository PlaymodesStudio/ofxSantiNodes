//
//  rateLimiter.h
//  ofxOceanode
//
//  Bridge node: accepts high-rate input, outputs at safe 60fps or slower
//  Use this before thread-unsafe nodes (GUI, OpenGL, etc.)
//  Includes speedlim-style rate reduction
//

#ifndef rateLimiter_h
#define rateLimiter_h

#include "ofxOceanodeNodeModel.h"
#include <mutex>
#include <chrono>

class rateLimiter : public ofxOceanodeNodeModel {
public:
	rateLimiter() : ofxOceanodeNodeModel("Rate Limiter") {
		lastOutputTime = 0;
	}
	
	void setup() {
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(minIntervalMs.set("SpeedlimMs", 0.0f, 0.0f, 1000.0f));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Listener fires at whatever rate input arrives (could be 240Hz, MIDI rate, etc.)
		listeners.push(input.newListener([this](vector<float> &vf){
			// Store latest data in thread-safe buffer
			std::lock_guard<std::mutex> lock(dataMutex);
			latestData = vf;
			hasNewData = true;
		}));
	}
	
	void update(ofEventArgs &args) override {
		// Check if we have new data
		bool shouldUpdate = false;
		vector<float> safeData;
		
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			if(hasNewData) {
				safeData = latestData;
				shouldUpdate = true;
				hasNewData = false;
			}
		}
		
		if(!shouldUpdate) return;
		
		// Apply rate limiting (speedlim behavior)
		uint64_t currentTime = getCurrentTimeMs();
		float minInterval = minIntervalMs.get();
		
		if(minInterval <= 0.0f) {
			// No rate limiting - output at 60fps
			output = safeData;
			lastOutputTime = currentTime;
		} else {
			// Check if enough time has elapsed since last output
			uint64_t elapsed = currentTime - lastOutputTime;
			
			if(elapsed >= minInterval) {
				// Enough time passed - allow output
				output = safeData;
				lastOutputTime = currentTime;
			}
			// else: too soon - skip this update (acts like speedlim)
		}
	}
	
private:
	ofEventListeners listeners;
	
	ofParameter<vector<float>> input;
	ofParameter<float> minIntervalMs;
	ofParameter<vector<float>> output;
	
	std::mutex dataMutex;
	vector<float> latestData;
	bool hasNewData = false;
	
	uint64_t lastOutputTime;
	
	uint64_t getCurrentTimeMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
		).count();
	}
};

#endif /* rateLimiter_h */
