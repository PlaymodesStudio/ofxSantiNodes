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
        //ofLogNotice("SnapshotClient") << "Saving client UUID: " << clientUUID;
    }
    
    // Load client UUID before other parameters
    void presetRecallBeforeSettingParameters(ofJson &json) override {
        if(json.contains("client_uuid")) {
            clientUUID = json["client_uuid"];
            //ofLogNotice("SnapshotClient") << "Restored client UUID: " << clientUUID;
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
                //ofLogNotice("SnapshotClient") << "Saving vector float value: " << response.value.dump();
                ofNotifyEvent(saveResponseEvent, response);
            }
            else if(sourceParam.valueType() == typeid(float).name()) {
                auto floatValue = sourceParam.cast<float>().getParameter().get();
                response.value = ofJson(vector<float>{floatValue}); // Convert to vector<float> for consistency
                //ofLogNotice("SnapshotClient") << "Saving float value: " << response.value.dump();
                ofNotifyEvent(saveResponseEvent, response);
            }
            else if(sourceParam.valueType() == typeid(vector<int>).name()) {
                auto vecValue = sourceParam.cast<vector<int>>().getParameter().get();
                vector<float> floatVec;
                floatVec.reserve(vecValue.size());
                for(auto val : vecValue) {
                    floatVec.push_back(static_cast<float>(val));
                }
                response.value = ofJson(floatVec);
                //ofLogNotice("SnapshotClient") << "Saving vector int value: " << response.value.dump();
                ofNotifyEvent(saveResponseEvent, response);
            }
            else if(sourceParam.valueType() == typeid(int).name()) {
                auto intValue = sourceParam.cast<int>().getParameter().get();
                response.value = ofJson(vector<float>{static_cast<float>(intValue)});
                //ofLogNotice("SnapshotClient") << "Saving int value: " << response.value.dump();
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
            vector<float> floatVec = e.value;
            
            // Get the connected input type to know how to convert the output
            if(!inputParam->hasInConnection()) {
                outputParam->cast<vector<float>>().getParameter() = floatVec;
                return;
            }
            
            auto inConnection = inputParam->getInConnection();
            if(inConnection == nullptr) {
                outputParam->cast<vector<float>>().getParameter() = floatVec;
                return;
            }
            
            auto &sourceParam = inConnection->getSourceParameter();
            string sourceType = sourceParam.valueType();
            
            // Convert output based on the input type
            if(sourceType == typeid(float).name()) {
                outputParam->cast<vector<float>>().getParameter() = vector<float>{floatVec[0]};
            }
            else if(sourceType == typeid(int).name()) {
                outputParam->cast<vector<float>>().getParameter() = vector<float>{static_cast<float>(static_cast<int>(floatVec[0]))};
            }
            else if(sourceType == typeid(vector<int>).name()) {
                vector<float> roundedVec;
                roundedVec.reserve(floatVec.size());
                for(auto val : floatVec) {
                    roundedVec.push_back(static_cast<float>(static_cast<int>(val)));
                }
                outputParam->cast<vector<float>>().getParameter() = roundedVec;
            }
            else {
                outputParam->cast<vector<float>>().getParameter() = floatVec;
            }
            
            //ofLogNotice("SnapshotClient") << "Retrieved value for " << e.parameterPath;
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
                    //ofLogNotice("SnapshotClient") << "Found server: " << server->getName();
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
            //ofLogNotice("SnapshotClient") << "Updated server list with " << serverNames.size() << " servers";
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
            vector<float> minVec(1, -FLT_MAX);
            vector<float> maxVec(1, FLT_MAX);
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
            vector<float> minVec(1, -FLT_MAX);
            vector<float> maxVec(1, FLT_MAX);
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
