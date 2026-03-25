#pragma once
#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class pitchClassLoad : public ofxOceanodeNodeModel {
public:
    pitchClassLoad() : ofxOceanodeNodeModel("Pitch Class Load") {}

    void setup() override {
        // Enumerate .txt files in data/PitchClass/
        fileNames = {"None"};
        ofDirectory dir;
        dir.open("PitchClass");
        if (dir.exists()) {
            dir.allowExt("txt");
            dir.listDir();
            dir.sort();
            for (int i = 0; i < (int)dir.size(); i++) {
                fileNames.push_back(dir.getName(i));
            }
        }

        // Preload all files
        for (int fi = 1; fi < (int)fileNames.size(); fi++) {
            allScales[fileNames[fi]] = parseFile("PitchClass/" + fileNames[fi]);
        }

        addParameterDropdown(fileIndex, "File", 0, fileNames);
        addParameterDropdown(scaleIndex, "Scale", 0, vector<string>{"---"});
        addParameter(fold.set("Fold", false));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(fileIndex.newListener([this](int &i) {
            onFileChanged(i);
        }));
        listeners.push(scaleIndex.newListener([this](int &) {
            compute();
        }));
        listeners.push(fold.newListener([this](bool &) {
            compute();
        }));
    }

    void loadBeforeConnections(ofJson &json) override {
        // Deserialize fileIndex first so onFileChanged rebuilds the dropdown options
        // before scaleIndex is restored, ensuring the correct option list is in place.
        deserializeParameter(json, fileIndex);
        deserializeParameter(json, scaleIndex);
    }

private:
    // Parse a PitchClass .txt file into a list of (name, values) pairs
    vector<pair<string, vector<float>>> parseFile(const string &path) {
        vector<pair<string, vector<float>>> result;
        ofFile file(path);
        if (!file.exists()) return result;
        ofBuffer buffer = file.readToBuffer();
        for (auto &line : buffer.getLines()) {
            if (line.empty()) continue;
            // Format: "index, name val1 val2 ...;"
            auto beforeSemicolon = ofSplitString(line, ";");
            if (beforeSemicolon.empty()) continue;
            auto commaSplit = ofSplitString(beforeSemicolon[0], ", ");
            if (commaSplit.size() < 2) continue;
            auto tokens = ofSplitString(commaSplit[1], " ");
            if (tokens.empty()) continue;
            string name = tokens[0];
            vector<float> vals;
            for (int i = 1; i < (int)tokens.size(); i++) {
                if (!tokens[i].empty())
                    vals.push_back(ofToFloat(tokens[i]));
            }
            result.emplace_back(name, vals);
        }
        return result;
    }

    void onFileChanged(int i) {
        currentScales.clear();

        vector<string> names;
        if (i > 0 && i < (int)fileNames.size() && allScales.count(fileNames[i])) {
            currentScales = allScales[fileNames[i]];
            for (auto &s : currentScales) names.push_back(s.first);
        }
        if (names.empty()) names = {"---"};

        scaleIndex.set(0);
        scaleIndex.setMin(0);
        scaleIndex.setMax(std::max(0, (int)names.size() - 1));
        getOceanodeParameter(scaleIndex).setDropdownOptions(names);

        compute();
    }

    void compute() {
        if (currentScales.empty() ||
            scaleIndex < 0 || scaleIndex >= (int)currentScales.size()) {
            output.set({0.0f});
            return;
        }
        vector<float> vals = currentScales[scaleIndex.get()].second;
        if (fold) {
            for (auto &v : vals) v = fmod(v, 12.0f);
            std::sort(vals.begin(), vals.end());
        }
        output.set(vals);
    }

    vector<string>                                      fileNames;
    map<string, vector<pair<string, vector<float>>>>    allScales;
    vector<pair<string, vector<float>>>                 currentScales;

    ofParameter<int>            fileIndex;
    ofParameter<int>            scaleIndex;
    ofParameter<bool>           fold;
    ofParameter<vector<float>>  output;

    ofEventListeners    listeners;
};
