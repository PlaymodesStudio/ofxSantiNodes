#ifndef vectorStorage_h
#define vectorStorage_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <map>
#include <limits>

class vectorStorage : public ofxOceanodeNodeModel {
public:
    vectorStorage() : ofxOceanodeNodeModel("Vector Storage") {
        description="Stores and recalls multiple vector presets, allowing real-time switching between stored states";
        addParameter(input.set("Input",
                               vector<float>(1, 0.0f),
                               vector<float>(1, -std::numeric_limits<float>::max()),
                               vector<float>(1, std::numeric_limits<float>::max())));
        addParameter(slot.set("Slot", 0, 0, 99));  // Allowing 100 slots (0-99)
        addParameter(store.set("Store"));
        addOutputParameter(output.set("Output",
                                      vector<float>(1, 0.0f),
                                      vector<float>(1, -std::numeric_limits<float>::max()),
                                      vector<float>(1, std::numeric_limits<float>::max())));

        listeners.push(store.newListener([this](){
            storeVector();
        }));

        listeners.push(slot.newListener([this](int &s){
            loadVector();
        }));
    }

    void presetSave(ofJson &json) override {
        // Save the entire storage map
        for (const auto& pair : storage) {
            json["storage"][ofToString(pair.first)] = pair.second;
        }
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        // Clear existing storage
        storage.clear();

        // Load the storage map
        if (json.count("storage") == 1) {
            for (auto it = json["storage"].begin(); it != json["storage"].end(); ++it) {
                int slot = ofToInt(it.key());
                storage[slot] = it.value().get<vector<float>>();
            }
        }

        // Load the vector for the current slot
        loadVector();
    }

private:
    ofParameter<vector<float>> input;
    ofParameter<int> slot;
    ofParameter<void> store;
    ofParameter<vector<float>> output;

    ofEventListeners listeners;

    std::map<int, vector<float>> storage;

    void storeVector() {
        int currentSlot = slot;
        storage[currentSlot] = input.get();
        ofLogNotice("Vector Storage") << "Stored vector in slot " << currentSlot;
        loadVector();  // Update output immediately after storing
    }

    void loadVector() {
        int currentSlot = slot;
        if (storage.find(currentSlot) != storage.end()) {
            output.set(storage[currentSlot]);
            ofLogNotice("Vector Storage") << "Loaded vector from slot " << currentSlot;
        } else {
            output.set(vector<float>(1, 0.0f));  // Set a default vector with one element
            ofLogNotice("Vector Storage") << "No vector stored in slot " << currentSlot;
        }
    }
};

#endif /* vectorStorage_h */
