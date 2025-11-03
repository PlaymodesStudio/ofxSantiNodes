#pragma once

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"

class cameraInput : public ofxOceanodeNodeModel {
public:
	// NOTE: explicitly initialize the base with a name (no default constructor)
	cameraInput()
	: ofxOceanodeNodeModel("Camera Input") {

		// UI / params
		addParameter(refreshDeviceList.set("Refresh Devices"));
		refreshDeviceList.addListener(this, &cameraInput::onRefreshDevices);

		addParameter(deviceID.set("Device ID", 0, 0, 0));
		deviceID.addListener(this, &cameraInput::onParamChanged);

		addParameter(width.set("Width", 640, 1, 3840));
		width.addListener(this, &cameraInput::onParamChanged);

		addParameter(height.set("Height", 480, 1, 2160));
		height.addListener(this, &cameraInput::onParamChanged);

		// Read-only info list with your vector init style
		addParameter(deviceNames.set("Devices", {""}, {""}, {""}));

		// Texture output (updated on new frames)
		addParameter(textureOut.set("Texture Out", nullptr));
	}

	~cameraInput() override {
		// remove listeners to avoid dangling callbacks on destruction
		refreshDeviceList.removeListener(this, &cameraInput::onRefreshDevices);
		deviceID.removeListener(this, &cameraInput::onParamChanged);
		width.removeListener(this, &cameraInput::onParamChanged);
		height.removeListener(this, &cameraInput::onParamChanged);

		grabber.close();
	}

	// ofxOceanode lifecycle
	void setup() override {
		refreshDevices();
		startGrabber();
	}

	void update(ofEventArgs &e) override {
		if(needsRestart){
			restartGrabber();
			needsRestart = false;
		}

		grabber.update();
		if(grabber.isFrameNew()){
			textureOut = &grabber.getTexture();
		}
	}

	void draw(ofEventArgs &e) override {}

private:
	// --- parameters ---
	ofParameter<void>            refreshDeviceList;
	ofParameter<int>             deviceID;
	ofParameter<int>             width;
	ofParameter<int>             height;
	ofParameter<vector<string>>  deviceNames;   // informational list
	ofParameter<ofTexture*>      textureOut;

	// --- internals ---
	ofVideoGrabber               grabber;
	vector<ofVideoDevice>        devices;
	bool                         needsRestart = false;

	// --- helpers ---
	void onRefreshDevices() {         // trigger listener
		refreshDevices();
		needsRestart = true;
	}

	void onParamChanged(int &){       // deviceID/width/height listeners
		needsRestart = true;
	}

	void refreshDevices(){
		devices = grabber.listDevices();

		vector<string> names;
		names.reserve(devices.size());
		for(const auto &d : devices){
			names.emplace_back(ofToString(d.id) + ": " + d.deviceName + (d.bAvailable ? "" : " (Unavailable)"));
		}

		if(names.empty()){
			names = { "No devices found" };
		}

		// update read-only list (keep your init style semantics)
		deviceNames = names;

		// keep deviceID range in-bounds
		if(!devices.empty()){
			deviceID.setMin(0);
			deviceID.setMax(static_cast<int>(devices.size()) - 1);
			deviceID = ofClamp(deviceID.get(), 0, static_cast<int>(devices.size()) - 1);
		}else{
			deviceID.setMin(0);
			deviceID.setMax(0);
			deviceID = 0;
		}
	}

	void startGrabber(){
		if(!devices.empty()){
			grabber.setDeviceID(deviceID.get());
		}
		grabber.setDesiredFrameRate(30); // reasonable default
		grabber.initGrabber(width.get(), height.get());
	}

	void restartGrabber(){
		grabber.close();
		startGrabber();
	}
};
