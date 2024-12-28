#ifndef snapshotEvents_h
#define snapshotEvents_h

#include "ofMain.h"

class snapshotServer;

struct ServerEvent {
    string uuid;
    bool active;
};

struct NameEvent {
    string uuid;
    string name;
};

struct SaveEvent {
    string serverUUID;
    int slot;
};

struct SaveResponse {
    string clientUUID;
    string parameterPath;
    ofJson value;
};

struct RetrieveEvent {
    string serverUUID;
    int slot;
    string clientUUID;
    string parameterPath;
    ofJson value;
};

static ofEvent<ServerEvent> serverEvent;
static ofEvent<NameEvent> serverNameEvent;
static ofEvent<vector<snapshotServer*>> getServersEvent;
static ofEvent<SaveEvent> saveRequestEvent;
static ofEvent<SaveResponse> saveResponseEvent;
static ofEvent<RetrieveEvent> retrieveEvent;

#endif