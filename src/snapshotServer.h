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
    }
    
    void setup() override {
        description = "Server node for storing/retrieving parameter snapshots";
        macroContext = getParents(); // Will be empty string if not in macro
        
        // Announce server existence - if no macro context, maintain global visibility
        ServerEvent e = {serverUUID, macroContext, true};
        ofNotifyEvent(serverEvent, e);
        
        addEventListeners();
    }
    
    void presetHasLoaded() override {
        // Re-announce server and setup listeners
        ServerEvent e = {serverUUID, macroContext, true};
        ofNotifyEvent(serverEvent, e);
        addEventListeners();
    }
    
    void deactivate() override {
        listeners.unsubscribeAll();
        
        // Announce server deactivation
        ServerEvent e = {serverUUID, macroContext, false};
        ofNotifyEvent(serverEvent, e);
    }
    
    void activate() override {
        // Re-announce server and setup listeners
        ServerEvent e = {serverUUID, macroContext, true};
        ofNotifyEvent(serverEvent, e);
        addEventListeners();
    }
    
    string getUUID() const { return serverUUID; }
    string getName() const { return serverName.get(); }
    
    void presetSave(ofJson &json) override {
        json["snapshot_data"] = snapshotData;
        json["server_uuid"] = serverUUID;
        json["server_name"] = serverName.get();
        json["macro_context"] = macroContext;
    }
    
    void presetRecallBeforeSettingParameters(ofJson &json) override {
        if(json.contains("server_uuid")) {
            serverUUID = json["server_uuid"];
        }
        if(json.contains("macro_context")) {
            macroContext = json["macro_context"];
        }
    }
    
    void presetRecallAfterSettingParameters(ofJson &json) override {
        if(json.contains("snapshot_data")) {
            snapshotData = json["snapshot_data"];
        }
        if(json.contains("server_name")) {
            serverName = json["server_name"];
        }
        
        // Announce server presence after loading
        ServerEvent e = {serverUUID, macroContext, true};
        ofNotifyEvent(serverEvent, e);
        
        // Announce name
        NameEvent nameEvt = {serverUUID, macroContext, serverName};
        ofNotifyEvent(serverNameEvent, nameEvt);
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
        slot = newSlot;
        SaveEvent saveEvent = {serverUUID, macroContext, newSlot};
        ofNotifyEvent(saveRequestEvent, saveEvent);
    }

    void onReplace() {
        SaveEvent saveEvent = {serverUUID, macroContext, slot};
        ofNotifyEvent(saveRequestEvent, saveEvent);
    }
    
    void onSlotChanged(int &newSlot) {
        onRetrieve();
    }
    
    void onRetrieve() {
        string slotKey = ofToString(slot.get());
        
        if(snapshotData.contains(slotKey)) {
            auto slotData = snapshotData[slotKey];
            for(auto it = slotData.begin(); it != slotData.end(); ++it) {
                RetrieveEvent e;
                e.serverUUID = serverUUID;
                e.macroContext = macroContext;
                e.slot = slot;
                e.clientUUID = it.key();
                e.parameterPath = it.value()["path"];
                e.value = it.value()["value"];
                ofNotifyEvent(retrieveEvent, e);
            }
        }
    }
    
    void handleSaveResponse(SaveResponse &response) {
        // Only handle responses from same macro context
        if(response.macroContext != macroContext) return;
        
        string slotKey = ofToString(slot);
        snapshotData[slotKey][response.clientUUID] = {
            {"path", response.parameterPath},
            {"value", response.value}
        };
    }
    
    void nameChanged(string &newName) {
        NameEvent e = {serverUUID, macroContext, newName};
        ofNotifyEvent(serverNameEvent, e);
    }
    
    void getAllServers(vector<snapshotServer*>& servers) {
        servers.push_back(this);
    }
    
private:
    mutable string serverUUID;
    string macroContext;
    ofParameter<string> serverName;
    ofParameter<int> slot;
    ofParameter<void> addButton;
    ofParameter<void> replaceButton;
    ofJson snapshotData;
    ofEventListeners listeners;
};

#endif
