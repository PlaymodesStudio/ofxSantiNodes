#pragma once

#include "ofMain.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace chordCypherAliases {
    inline std::string getAliasFilePath() {
        return ofToDataPath("Supercollider/Pitchclass/cypher_aliases.json", true);
    }

    inline std::string normalizeLookupKey(const std::string &value) {
        std::string normalized = ofTrim(value);
        normalized.erase(std::remove(normalized.begin(), normalized.end(), ' '), normalized.end());
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return normalized;
    }

    inline size_t mergeAliasesFromFile(std::unordered_map<std::string, std::string> &aliases,
                                       const std::string &logTag) {
        std::string aliasPath = getAliasFilePath();
        ofFile aliasFile(aliasPath);
        if(!aliasFile.exists()) return 0;

        ofJson json = ofLoadJson(aliasPath);
        if(!json.is_object()) {
            ofLogWarning(logTag) << "Cypher alias file is not a JSON object: " << aliasPath;
            return 0;
        }

        size_t loadedCount = 0;
        for(auto &[key, value] : json.items()) {
            if(!value.is_string()) continue;
            aliases[key] = value.get<std::string>();
            aliases[normalizeLookupKey(key)] = value.get<std::string>();
            loadedCount++;
        }

        return loadedCount;
    }

    inline std::string resolveAlias(const std::unordered_map<std::string, std::string> &aliases,
                                    const std::string &value) {
        auto exactIt = aliases.find(value);
        if(exactIt != aliases.end()) return exactIt->second;

        auto normalizedIt = aliases.find(normalizeLookupKey(value));
        if(normalizedIt != aliases.end()) return normalizedIt->second;

        return value;
    }
}
