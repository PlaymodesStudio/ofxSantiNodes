#ifndef snapshotClient_h
#define snapshotClient_h

#include "ofxOceanodeNodeModel.h"
#include "snapshotServer.h"
#include "snapshotEvents.h"

class snapshotClient : public ofxOceanodeNodeModel {
public:
    snapshotClient() : ofxOceanodeNodeModel("Snapshot Client") {
        color = ofColor::cyan;
        
        // Initialize UUID
        clientUUID = ofToString(ofRandomuf());
        
        // Create separate input and output parameters
        inputParam = createInputParam<vector<float>>();
        outputParam = createOutputParam<vector<float>>();
        
        // Initialize with empty server list
        serverNames = {"No Servers"};
        serverParam = addParameterDropdown(serverSelector, "Server", 0, serverNames);
    }
    
    void setup() override {
        description = "Client node for parameter snapshots";
        addEventListeners();
        refreshServerList();
    }
    
    void presetHasLoaded() override {
        refreshServerList();
    }

    void deactivate() override {
        listeners.unsubscribeAll();
    }

    void activate() override {
        addEventListeners();
        refreshServerList();
    }
    
    // Save client UUID with preset
    void presetSave(ofJson &json) override {
        json["client_uuid"] = clientUUID;
        ofLogNotice("SnapshotClient") << "Saving client UUID: " << clientUUID;
    }
    
    // Load client UUID before other parameters
    void presetRecallBeforeSettingParameters(ofJson &json) override {
        if(json.contains("client_uuid")) {
            clientUUID = json["client_uuid"];
            ofLogNotice("SnapshotClient") << "Restored client UUID: " << clientUUID;
        }
    }
    
private:
    void addEventListeners() {
        listeners.unsubscribeAll(); // Clear any existing listeners first
        listeners.push(serverEvent.newListener(this, &snapshotClient::serverListChanged));
        listeners.push(serverNameEvent.newListener(this, &snapshotClient::serverNameChanged));
        listeners.push(saveRequestEvent.newListener(this, &snapshotClient::handleSaveRequest));
        listeners.push(retrieveEvent.newListener(this, &snapshotClient::handleRetrieveEvent));
    }
    
    void handleSaveRequest(SaveEvent &e) {
        if(!serverParam || serverSelector.get() >= serverUUIDs.size()) return;
        
        if(serverUUIDs[serverSelector] == e.serverUUID) {
            SaveResponse response;
            response.clientUUID = clientUUID;
            response.parameterPath = getConnectedParameterPath();
            
            if(!inputParam->hasInConnection()) return;
            auto inConnection = inputParam->getInConnection();
            if(inConnection == nullptr) return;
            
            auto &sourceParam = inConnection->getSourceParameter();
            if(sourceParam.valueType() == typeid(vector<float>).name()) {
                auto vecValue = sourceParam.cast<vector<float>>().getParameter().get();
                response.value = ofJson(vecValue);
                ofLogNotice("SnapshotClient") << "Saving vector value: " << response.value.dump();
                ofNotifyEvent(saveResponseEvent, response);
            }
            else if(sourceParam.valueType() == typeid(float).name()) {
                auto floatValue = sourceParam.cast<float>().getParameter().get();
                response.value = ofJson(vector<float>{floatValue}); // Convert to vector<float> for consistency
                ofLogNotice("SnapshotClient") << "Saving float value: " << response.value.dump();
                ofNotifyEvent(saveResponseEvent, response);
            }
            else {
                ofLogNotice("SnapshotClient") << "Unsupported parameter type: " << sourceParam.valueType();
            }
        }
    }
    
    string getConnectedParameterPath() {
        if(!inputParam->hasInConnection()) return "";
        auto inConnection = inputParam->getInConnection();
        if(inConnection == nullptr) return "";
        auto &sourceParam = inConnection->getSourceParameter();
        auto nodeModel = sourceParam.getNodeModel();
        return nodeModel->nodeName() + "/" + sourceParam.getName();
    }
    
    void handleRetrieveEvent(RetrieveEvent &e) {
        if(e.clientUUID == clientUUID && serverUUIDs[serverSelector] == e.serverUUID) {
            auto &param = outputParam->cast<vector<float>>();
            vector<float> vecValue = e.value;
            param.getParameter() = vecValue;
            ofLogNotice("SnapshotClient") << "Retrieved value for " << e.parameterPath;
        }
    }
    
    void refreshServerList() {
        vector<snapshotServer*> servers;
        ofNotifyEvent(getServersEvent, servers);
        
        vector<string> newServerUUIDs;
        vector<string> newServerNames;
        
        if(!servers.empty()) {
            for(auto server : servers) {
                if(server) {
                    newServerUUIDs.push_back(server->getUUID());
                    newServerNames.push_back(server->getName());
                    ofLogNotice("SnapshotClient") << "Found server: " << server->getName();
                }
            }
        }
        
        // Update stored lists
        serverUUIDs = newServerUUIDs;
        serverNames = newServerNames;
        
        if(serverNames.empty()) {
            serverNames = {"No Servers"};
            serverSelector.setMax(0);
        } else {
            serverSelector.setMax(serverNames.size() - 1);
        }
        
        // Update dropdown options
        if(serverParam) {
            serverParam->setDropdownOptions(serverNames);
            ofLogNotice("SnapshotClient") << "Updated server list with " << serverNames.size() << " servers";
        }
    }
    
    void serverListChanged(ServerEvent &e) {
        refreshServerList();
    }
    
    void serverNameChanged(NameEvent &e) {
        refreshServerList();
    }
    
    template<typename T>
    shared_ptr<ofxOceanodeParameter<T>> createInputParam() {
        ofParameter<T> param;
        if constexpr (std::is_same_v<T, vector<float>>) {
            vector<float> defaultVec(1, 0.0f);
            vector<float> minVec(1, 0.0f);
            vector<float> maxVec(1, 1.0f);
            param.set("Input", defaultVec, minVec, maxVec);
        } else {
            param.set("Input", T(), T(), T());
        }
        return addParameter(param);
    }
    
    template<typename T>
    shared_ptr<ofxOceanodeParameter<T>> createOutputParam() {
        ofParameter<T> param;
        if constexpr (std::is_same_v<T, vector<float>>) {
            vector<float> defaultVec(1, 0.0f);
            vector<float> minVec(1, 0.0f);
            vector<float> maxVec(1, 1.0f);
            param.set("Output", defaultVec, minVec, maxVec);
        } else {
            param.set("Output", T(), T(), T());
        }
        // Make it an output-only parameter
        auto paramPtr = addParameter(param, ofxOceanodeParameterFlags_DisableInConnection);
        return paramPtr;
    }
    
private:
    string clientUUID;
    ofParameter<int> serverSelector;
    shared_ptr<ofxOceanodeParameter<int>> serverParam;
    shared_ptr<ofxOceanodeParameter<vector<float>>> inputParam;
    shared_ptr<ofxOceanodeParameter<vector<float>>> outputParam;
    vector<string> serverUUIDs;
    vector<string> serverNames;
    ofEventListeners listeners;
};

#endif
