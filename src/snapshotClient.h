#ifndef snapshotClient_h
#define snapshotClient_h

#include "ofxOceanodeNodeModel.h"
#include "snapshotServer.h"
#include "snapshotEvents.h"

class snapshotClient : public ofxOceanodeNodeModel {
public:
    snapshotClient() : ofxOceanodeNodeModel("Snapshot Client") {
        color = ofColor::cyan;
        
        // Initialize with empty server list
        serverNames = {"No Servers"};
        serverParam = addParameterDropdown(serverSelector, "Server", 0, serverNames);
        
        // Create separate input and output parameters
        inputParam = createInputParam<vector<float>>();
        outputParam = createOutputParam<vector<float>>();
        
        selectedServerName = "";
    }
    
    void setup() override {
        description = "Client node for parameter snapshots";
        
        macroContext = getParents();
        generateUUID();
        
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
    
    void presetSave(ofJson &json) override {
        json["server_name"] = selectedServerName;
        json["client_uuid"] = clientUUID;
        json["macro_context"] = macroContext;
    }
    
    void presetRecallAfterSettingParameters(ofJson &json) override {
        if(json.contains("server_name")) {
            string savedServerName = json["server_name"];
            // Find the server by name in our current list
            for(size_t i = 0; i < serverNames.size(); i++) {
                if(serverNames[i] == savedServerName) {
                    serverSelector = i;
                    selectedServerName = savedServerName;
                    break;
                }
            }
        }
        
        if(json.contains("client_uuid")) {
            clientUUID = json["client_uuid"];
        }
        
        if(json.contains("macro_context")) {
            macroContext = json["macro_context"];
        }
        
        refreshServerList();
    }
    
private:
    void serverChanged(int &index) {
        if(index >= 0 && index < serverNames.size()) {
            selectedServerName = serverNames[index];
            generateUUID();
        }
    }
    
    void generateUUID() {
        string groupName = getParameterGroup().getName();
        string idStr = ofSplitString(groupName, " ").back();
        
        if(selectedServerName.empty()) {
            selectedServerName = "no_server";
        }
        
        clientUUID = "client_" + idStr + "_" + selectedServerName;
    }
    
    void addEventListeners() {
        listeners.unsubscribeAll();
        listeners.push(serverEvent.newListener(this, &snapshotClient::serverListChanged));
        listeners.push(serverNameEvent.newListener(this, &snapshotClient::serverNameChanged));
        listeners.push(saveRequestEvent.newListener(this, &snapshotClient::handleSaveRequest));
        listeners.push(retrieveEvent.newListener(this, &snapshotClient::handleRetrieveEvent));
        listeners.push(serverSelector.newListener(this, &snapshotClient::serverChanged));
    }
    
    void handleSaveRequest(SaveEvent &e) {
        // Only respond to events from same macro context
        if(e.macroContext != macroContext) return;
        
        if(!serverParam || serverSelector.get() >= serverUUIDs.size()) return;
        
        if(serverUUIDs[serverSelector] == e.serverUUID) {
            SaveResponse response;
            response.clientUUID = clientUUID;
            response.macroContext = macroContext;
            response.parameterPath = getConnectedParameterPath();
            
            if(!inputParam->hasInConnection()) return;
            auto inConnection = inputParam->getInConnection();
            if(inConnection == nullptr) return;
            
            auto &sourceParam = inConnection->getSourceParameter();
            
            // Handle different parameter types while preserving original values
            if(sourceParam.valueType() == typeid(vector<float>).name()) {
                auto vecValue = sourceParam.cast<vector<float>>().getParameter().get();
                response.value = ofJson(vecValue);
            }
            else if(sourceParam.valueType() == typeid(float).name()) {
                float floatValue = sourceParam.cast<float>().getParameter().get();
                response.value = ofJson(vector<float>{floatValue});
            }
            else if(sourceParam.valueType() == typeid(vector<int>).name()) {
                auto vecValue = sourceParam.cast<vector<int>>().getParameter().get();
                vector<float> floatVec;
                floatVec.reserve(vecValue.size());
                for(auto val : vecValue) {
                    floatVec.push_back(static_cast<float>(val));
                }
                response.value = ofJson(floatVec);
            }
            else if(sourceParam.valueType() == typeid(int).name()) {
                int intValue = sourceParam.cast<int>().getParameter().get();
                response.value = ofJson(vector<float>{static_cast<float>(intValue)});
            }
            
            ofNotifyEvent(saveResponseEvent, response);
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
        // Only process events from same macro context
        if(e.macroContext != macroContext) return;
        
        if(e.clientUUID == clientUUID && serverUUIDs[serverSelector] == e.serverUUID) {
            vector<float> floatVec = e.value;
            
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
            
            if(sourceType == typeid(float).name()) {
                outputParam->cast<vector<float>>().getParameter() = vector<float>{floatVec[0]};
            }
            else if(sourceType == typeid(int).name()) {
                outputParam->cast<vector<float>>().getParameter() = vector<float>{round(floatVec[0])};
            }
            else if(sourceType == typeid(vector<int>).name()) {
                vector<float> roundedVec;
                roundedVec.reserve(floatVec.size());
                for(auto val : floatVec) {
                    roundedVec.push_back(round(val));
                }
                outputParam->cast<vector<float>>().getParameter() = roundedVec;
            }
            else {
                outputParam->cast<vector<float>>().getParameter() = floatVec;
            }
        }
    }
    
    void refreshServerList() {
        vector<snapshotServer*> servers;
        ofNotifyEvent(getServersEvent, servers);
        
        vector<string> newServerUUIDs;
        vector<string> newServerNames;
        
        if(!servers.empty()) {
            for(auto server : servers) {
                // Add servers if:
                // - We're not in a macro (macroContext empty) OR
                // - Server is in same macro context
                if(server && (macroContext.empty() || server->getParents() == macroContext)) {
                    newServerUUIDs.push_back(server->getUUID());
                    newServerNames.push_back(server->getName());
                }
            }
        }
        
        serverUUIDs = newServerUUIDs;
        serverNames = newServerNames;
        
        if(serverNames.empty()) {
            serverNames = {"No Servers"};
            serverSelector.setMax(0);
        } else {
            serverSelector.setMax(serverNames.size() - 1);
        }
        
        if(serverParam) {
            serverParam->setDropdownOptions(serverNames);
        }
    }
    
    void serverListChanged(ServerEvent &e) {
        // If we're not in a macro, or event is from same macro context, refresh
        if(macroContext.empty() || e.macroContext == macroContext) {
            refreshServerList();
        }
    }
    
    void serverNameChanged(NameEvent &e) {
        // Only refresh if event is from same macro context
        if(e.macroContext == macroContext) {
            refreshServerList();
        }
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
        auto paramPtr = addParameter(param, ofxOceanodeParameterFlags_DisableInConnection);
        return paramPtr;
    }
    
private:
    string clientUUID;
    string macroContext;
    ofParameter<int> serverSelector;
    shared_ptr<ofxOceanodeParameter<int>> serverParam;
    shared_ptr<ofxOceanodeParameter<vector<float>>> inputParam;
    shared_ptr<ofxOceanodeParameter<vector<float>>> outputParam;
    vector<string> serverUUIDs;
    vector<string> serverNames;
    string selectedServerName;
    ofEventListeners listeners;
};

#endif
