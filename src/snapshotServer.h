#ifndef snapshotServer_h
#define snapshotServer_h

#include "ofxOceanodeNodeModel.h"
#include "ofJson.h"
#include "snapshotEvents.h"

class snapshotServer : public ofxOceanodeNodeModel {
public:
    snapshotServer() : ofxOceanodeNodeModel("Snapshot Server"),
                      serverUUID(ofToString(ofRandomuf())) {
        color = ofColor::darkCyan;
        
        // Initialize basic parameters
        addParameter(serverName.set("Name", "Server 1"));
        addParameter(slot.set("Snapshot", 0, 0, 127));
        addParameter(addButton.set("Add"));
        addParameter(replaceButton.set("Replace"));
        
        //ofLogNotice("SnapshotServer") << "Created server with UUID: " << serverUUID;
    }
    
    void setup() override {
        description = "Server node for storing/retrieving parameter snapshots";
        
        // Announce server existence
        ServerEvent e = {serverUUID, true};
        ofNotifyEvent(serverEvent, e);
        
        addEventListeners();
    }
    
    void presetHasLoaded() override {
        // Re-announce server and setup listeners
        ServerEvent e = {serverUUID, true};
        ofNotifyEvent(serverEvent, e);
        addEventListeners();
    }
    
    void deactivate() override {
        listeners.unsubscribeAll();
        
        // Announce server deactivation
        ServerEvent e = {serverUUID, false};
        ofNotifyEvent(serverEvent, e);
    }
    
    void activate() override {
        // Re-announce server and setup listeners
        ServerEvent e = {serverUUID, true};
        ofNotifyEvent(serverEvent, e);
        addEventListeners();
    }
    
    string getUUID() const { return serverUUID; }
    string getName() const { return serverName.get(); }
    
    void presetSave(ofJson &json) override {
        json["snapshot_data"] = snapshotData;
        json["server_uuid"] = serverUUID;
        json["server_name"] = serverName.get();
        //ofLogNotice("SnapshotServer") << "Saving preset with data: " << snapshotData.dump(2);
    }
    
    void presetRecallBeforeSettingParameters(ofJson &json) override {
        if(json.contains("server_uuid")) {
            serverUUID = json["server_uuid"];
            //ofLogNotice("SnapshotServer") << "Restored server UUID: " << serverUUID;
        }
    }
    
    void presetRecallAfterSettingParameters(ofJson &json) override {
        if(json.contains("snapshot_data")) {
            snapshotData = json["snapshot_data"];
            //ofLogNotice("SnapshotServer") << "Loaded snapshot data: " << snapshotData.dump(2);
        }
    }
    
private:
    void addEventListeners() {
        listeners.unsubscribeAll(); // Clear any existing listeners first
        listeners.push(addButton.newListener(this, &snapshotServer::onAdd));
        listeners.push(replaceButton.newListener(this, &snapshotServer::onReplace));
        listeners.push(slot.newListener(this, &snapshotServer::onSlotChanged));
        listeners.push(serverName.newListener(this, &snapshotServer::nameChanged));
        listeners.push(saveResponseEvent.newListener(this, &snapshotServer::handleSaveResponse));
        listeners.push(getServersEvent.newListener(this, &snapshotServer::getAllServers));
    }

    int findFirstAvailableSlot() {
        int maxSlot = -1;
        for(auto& element : snapshotData.items()) {
            int currentSlot = ofToInt(element.key());
            maxSlot = std::max(maxSlot, currentSlot);
        }
        return maxSlot + 1;
    }

    void onAdd() {
        int newSlot = findFirstAvailableSlot();
        slot = newSlot; // This will trigger onSlotChanged, but with empty data
        //ofLogNotice("SnapshotServer") << "Adding new snapshot in slot " << newSlot;
        SaveEvent saveEvent = {serverUUID, newSlot};
        ofNotifyEvent(saveRequestEvent, saveEvent);
    }

    void onReplace() {
        //ofLogNotice("SnapshotServer") << "Replacing slot " << slot;
        SaveEvent saveEvent = {serverUUID, slot};
        ofNotifyEvent(saveRequestEvent, saveEvent);
    }
    
    void onSlotChanged(int &newSlot) {
        // Load the new slot automatically
        onRetrieve();
    }
    
    void onRetrieve() {
        string slotKey = ofToString(slot.get());
        //ofLogNotice("SnapshotServer") << "Retrieving slot " << slotKey;
        
        if(snapshotData.contains(slotKey)) {
            auto slotData = snapshotData[slotKey];
            for(auto it = slotData.begin(); it != slotData.end(); ++it) {
                RetrieveEvent e;
                e.serverUUID = serverUUID;
                e.slot = slot;
                e.clientUUID = it.key();
                e.parameterPath = it.value()["path"];
                e.value = it.value()["value"];
                ofNotifyEvent(retrieveEvent, e);
                //ofLogNotice("SnapshotServer") << "Sending retrieve event for client: " << e.clientUUID;
            }
        } else {
            //ofLogNotice("SnapshotServer") << "No data found for slot " << slotKey;
        }
    }
    
    void handleSaveResponse(SaveResponse &response) {
        string slotKey = ofToString(slot);
        //ofLogNotice("SnapshotServer") << "Handling save response from client: " << response.clientUUID;
        //ofLogNotice("SnapshotServer") << "Path: " << response.parameterPath;
        //ofLogNotice("SnapshotServer") << "Value: " << response.value;
        
        snapshotData[slotKey][response.clientUUID] = {
            {"path", response.parameterPath},
            {"value", response.value}
        };
        
        //ofLogNotice("SnapshotServer") << "Updated snapshot data: " << snapshotData.dump(2);
    }
    
    void nameChanged(string &newName) {
        NameEvent e = {serverUUID, newName};
        ofNotifyEvent(serverNameEvent, e);
        //ofLogNotice("SnapshotServer") << "Name changed to: " << newName;
    }
    
    void getAllServers(vector<snapshotServer*>& servers) {
        servers.push_back(this);
        //ofLogNotice("SnapshotServer") << "Responding to get servers request";
    }
    
private:
    mutable string serverUUID;
    ofParameter<string> serverName;
    ofParameter<int> slot;
    ofParameter<void> addButton;
    ofParameter<void> replaceButton;
    ofJson snapshotData;
    ofEventListeners listeners;
};

#endif
