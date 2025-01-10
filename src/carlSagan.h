#pragma once

    #include "ofxOceanodeNodeModel.h"
    #include "ofUtils.h"
    #include <map>
    #include <vector>
    #include <string>
    #include <random>

    class NarrativeManager {
    public:
        static NarrativeManager& getInstance() {
            static NarrativeManager instance;
            return instance;
        }

        void init(const string& basePath) {
            if (!initialized) {
                loadNarratives(basePath);
                loadColors(basePath);
                loadStarTypes(basePath);
                initialized = true;
            }
        }

        void loadNarratives(const string& basePath) {
            vector<string> languages = {"ca", "en"};
            vector<string> styles = {"minimal", "sagan"};

            for (const auto& lang : languages) {
                for (const auto& style : styles) {
                    string filePath = ofToDataPath(basePath + "/" + lang + "_" + style + ".txt", true);
                    ofBuffer buffer = ofBufferFromFile(filePath);
                    if (buffer.size() > 0) {
                        narratives[lang + "_" + style] = ofSplitString(buffer.getText(), "\n");
                        //ofLogNotice("NarrativeManager") << "Loaded file: " << filePath;
                    } else {
                        ofLogError("NarrativeManager") << "Could not load file: " << filePath;
                    }
                }
            }
        }


        string getNarrative(const string& language, const string& style, bool randomize = false) {
            string key = language + "_" + style;
            if (narratives.find(key) != narratives.end()) {
                const auto& lines = narratives[key];
                if (!lines.empty()) {
                    if (randomize) {
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_int_distribution<> dis(0, lines.size() - 1);
                        return lines[dis(gen)];
                    } else {
                        return lines[0];
                    }
                }
            }
            ofLogError("NarrativeManager") << "No narrative found for key: " << key;
            return "";
        }

        string getColorDescription(float bvColor, const string& language) {
            string key = language;
            if (colors.find(key) != colors.end()) {
                for (const auto& range : colors[key]) {
                    if (bvColor >= range.first.first && bvColor < range.first.second) {
                        return range.second;
                    }
                }
            }
            return "unknown";
        }

        string getStarType(const string& spectralType, const string& subtype, const string& language) {
                string key = language;
                if (starTypes.find(key) == starTypes.end()) {
                    return "unknown";
                }

                // Normalitzem la subkey abans de fer la cerca
                string normalizedSubtype = normalizeSubKey(subtype);
                
                // Intentem amb la combinació normalitzada
                string lookupKey = spectralType + ":" + normalizedSubtype;
                //ofLogNotice("getStarType") << "Trying normalized lookup: " << lookupKey;
                
                if (starTypes[key].count(lookupKey) > 0) {
                    return starTypes[key][lookupKey];
                }

                // Si no funciona, provem amb el default
                lookupKey = spectralType + ":default";
                if (starTypes[key].count(lookupKey) > 0) {
                    return starTypes[key][lookupKey];
                }

                return "unknown";
            }

        

    private:
        
        string normalizeSubKey(const string& subKey) {
                // Mapa de normalització (podem expandir-lo segons necessitem)
                static const map<string, string> normalization = {
                    {"subgiants", "giants"},    // IV -> giants
                    {"subdwarfs", "dwarfs"},    // VI -> dwarfs
                    {"white dwarfs", "dwarfs"}, // VII -> dwarfs
                    {"bright giants", "giants"}, // II -> giants
                };

                // Si la subkey està al mapa de normalització, usem el valor normalitzat
                if (normalization.find(subKey) != normalization.end()) {
                    //ofLogNotice("normalizeSubKey") << "Normalized " << subKey << " to " << normalization.at(subKey);
                    return normalization.at(subKey);
                }

                // Si no, retornem la subkey original
                return subKey;
            }
        
        void loadColors(const string& basePath) {
            vector<string> languages = {"ca", "en"};
            for (const auto& lang : languages) {
                string filePath = ofToDataPath(basePath + "/" + "colors_" + lang + ".txt", true);
                ofBuffer buffer = ofBufferFromFile(filePath);
                if (buffer.size() > 0) {
                    auto lines = ofSplitString(buffer.getText(), "\n");
                    //ofLogNotice("NarrativeManager: colors") << "Loaded file: " << filePath;

                    for (const auto& line : lines) {
                        auto parts = ofSplitString(line, ":");
                        if (parts.size() == 2) {
                            auto range = ofSplitString(parts[0], ",");
                            if (range.size() == 2) {
                                colors[lang].emplace_back(
                                    std::make_pair(ofToFloat(range[0]), ofToFloat(range[1])),
                                    parts[1]
                                );
                            }
                        }
                    }
                } else {
                    ofLogError("NarrativeManager") << "Could not load file: " << filePath;
                }
            }
        }


        void loadStarTypes(const string& basePath) {
            vector<string> languages = {"ca", "en"};
            for (const auto& lang : languages) {
                string filePath = ofToDataPath(basePath + "/" + "startypes_" + lang + ".txt", true);
                ofBuffer buffer = ofBufferFromFile(filePath);
                if (buffer.size() > 0) {
                    auto lines = ofSplitString(buffer.getText(), "\n");
                    //ofLogNotice("NarrativeManager: startypes") << "Loading file: " << filePath;

                    for (const auto& line : lines) {
                        auto parts = ofSplitString(line, ":");
                        if (parts.size() == 3) {
                            string key = parts[0] + ":" + parts[1];
                            starTypes[lang][key] = parts[2];
                            //ofLogNotice("NarrativeManager: startypes") << "Loaded: " << key << " -> " << parts[2];
                        }
                    }

                    //ofLogNotice("NarrativeManager: startypes") << "Loaded " << starTypes[lang].size()
                                                            // << " entries for " << lang;
                } else {
                    ofLogError("NarrativeManager") << "Could not load file: " << filePath;
                }
            }
        }


        NarrativeManager() : initialized(false) {} // Constructor privat

        map<string, vector<string>> narratives;
        map<string, vector<pair<pair<float, float>, string>>> colors;
        map<string, map<string, string>> starTypes;
        bool initialized;
    };

    class carlSagan : public ofxOceanodeNodeModel {
    public:
        carlSagan() : ofxOceanodeNodeModel("Carl Sagan") {
                string basePath = "catalog/narratives";
                NarrativeManager::getInstance().init(basePath);
                setupParams();
            }
        


        void setupParams() {
            // Inputs
            parallaxIn.set("Parallax In", 0.0f, -100.0f, 100.0f);
            magnitudeIn.set("Magnitude In", 0.0f, -27.0f, 20.0f);
            bvColorIn.set("Color In", 0.0f, -1.0f, 3.0f);
            spectralTypeIn.set("SpecType In", "");
            sunXIn.set("Sun Mass X", 1.0f, 0.1f, 150.0f);
            multipleCountIn.set("Multiple In", 1, 1, 10);
            starNameIn.set("Name In", "");
            constellationIn.set("Constel In", "");

            // Outputs
            narrativeOut.set("Narrative", "");

            // Dropdowns
            languageParam.set("Language", 0, 0, 1); // 0: Català, 1: English
            styleParam.set("Style", 0, 0, 1); // 0: Minimal, 1: Sagan

            // Add parameters
            addParameter(parallaxIn);
            addParameter(magnitudeIn);
            addParameter(bvColorIn);
            addParameter(spectralTypeIn);
            addParameter(multipleCountIn);
            addParameter(starNameIn);
            addParameter(constellationIn);
            addParameter(sunXIn);
            addParameterDropdown(languageParam, "Language", 0, {"Català", "English"});
            addParameterDropdown(styleParam, "Style", 0, {"Minimal", "Sagan"});
            addParameter(narrativeOut);

            setupListeners();
        }

    private:
        void setupListeners() {
            uint64_t lastUpdateTime = 0;
            const uint64_t minUpdateInterval = 100; // 100ms mínim entre updates

            auto updateWithThrottle = [this, &lastUpdateTime]() {
                uint64_t currentTime = ofGetElapsedTimeMillis();
                if(currentTime - lastUpdateTime > minUpdateInterval) {
                    updateNarrative();
                    lastUpdateTime = currentTime;
                }
            };

            listeners.push(parallaxIn.newListener([updateWithThrottle](float&) { updateWithThrottle(); }));
            listeners.push(magnitudeIn.newListener([updateWithThrottle](float&) { updateWithThrottle(); }));
            listeners.push(bvColorIn.newListener([updateWithThrottle](float&) { updateWithThrottle(); }));
            listeners.push(spectralTypeIn.newListener([updateWithThrottle](string&) { updateWithThrottle(); }));
            listeners.push(multipleCountIn.newListener([updateWithThrottle](int&) { updateWithThrottle(); }));
            listeners.push(starNameIn.newListener([updateWithThrottle](string&) { updateWithThrottle(); }));
            listeners.push(constellationIn.newListener([updateWithThrottle](string&) { updateWithThrottle(); }));
            listeners.push(sunXIn.newListener([updateWithThrottle](float&) { updateWithThrottle(); }));
            listeners.push(languageParam.newListener([updateWithThrottle](int&) { updateWithThrottle(); }));
            listeners.push(styleParam.newListener([updateWithThrottle](int&) { updateWithThrottle(); }));
        }
        
        struct SpectralTypeComponents {
                string classKey;
                string subKey;
            };

        SpectralTypeComponents parseSpectralType(const string& spectralType) {
            SpectralTypeComponents components;
            
            if (spectralType.empty()) {
                components.classKey = "default";
                components.subKey = "default";
                return components;
            }

            // Handle special case of binary/composite systems - use only primary
            size_t plusPos = spectralType.find("+");
            string primaryType = (plusPos != string::npos) ?
                                spectralType.substr(0, plusPos) : spectralType;

            // Clean the type first
            string cleanType = primaryType;
            
            // Extract spectral class (first letter)
            components.classKey = cleanType.substr(0, 1);

            // Convert to uppercase for matching
            string upperType = ofToUpper(cleanType);

            // Remove uncertain marks
            size_t colonPos = upperType.find(":");
            if (colonPos != string::npos) {
                upperType = upperType.substr(0, colonPos);
            }

            // Remove various spectral peculiarities
            vector<string> peculiarities = {
                "P", "E", "EU", "SR", "SI", "CR", "CN", "FE", "BA", "ZR", "S",
                "N", "HC", "MN", "CA", "TI", "V", "M", "W"
            };
            
            for (const auto& p : peculiarities) {
                size_t pos = upperType.find(p);
                if (pos != string::npos) {
                    // Only remove if it's after any luminosity class indicators
                    if (!isdigit(upperType[pos-1]) &&
                        upperType.substr(0, pos).find("III") == string::npos &&
                        upperType.substr(0, pos).find("II") == string::npos &&
                        upperType.substr(0, pos).find("IV") == string::npos &&
                        upperType.substr(0, pos).find("V") == string::npos) {
                        upperType = upperType.substr(0, pos);
                    }
                }
            }

            // Remove 'e' for emission lines if it's not part of a luminosity indicator
            size_t ePos = upperType.find("E");
            if (ePos != string::npos && ePos > 0) {
                if (upperType[ePos-1] != 'P' && upperType[ePos-1] != 'F') {
                    upperType.erase(ePos, 1);
                }
            }

            // Default subKey
            components.subKey = "default";

            // Check for luminosity classes in order of specificity
            if (upperType.find("IA-O") != string::npos ||
                upperType.find("IA") != string::npos ||
                upperType.find("IB") != string::npos ||
                ((upperType.find("I") != string::npos || upperType.find("1") != string::npos) &&
                 upperType.find("II") == string::npos &&
                 upperType.find("III") == string::npos &&
                 upperType.find("IV") == string::npos &&
                 upperType.find("V") == string::npos)) {
                components.subKey = "supergiants";
            }
            else if (upperType.find("III") != string::npos) {
                components.subKey = "giants";
            }
            else if (upperType.find("II") != string::npos) {
                // II stars are generally closer to giants in properties
                components.subKey = "giants";
            }
            else if (upperType.find("IV-V") != string::npos ||
                     upperType.find("IV/V") != string::npos) {
                components.subKey = "dwarfs";
            }
            else if (upperType.find("IV") != string::npos) {
                // Subgiants grouped with giants
                components.subKey = "giants";
            }
            else if (upperType.find("V") != string::npos ||
                     upperType.find("VI") != string::npos ||
                     upperType.find("VII") != string::npos) {
                components.subKey = "dwarfs";
            }

            // Handle shell stars and peculiar cases
            if (upperType.find("SHELL") != string::npos) {
                if (components.subKey == "default") {
                    components.subKey = "dwarfs";  // Shell stars are often main sequence
                }
            }

            // If still no luminosity class found, use some reasonable defaults
            if (components.subKey == "default") {
                if (components.classKey == "G" ||
                    components.classKey == "K" ||
                    components.classKey == "M") {
                    components.subKey = "dwarfs";  // These are typically dwarfs if unspecified
                }
                // For early type stars (O, B, A), keep as "default" to use general description
            }

            return components;
        }



        void updateNarrative() {
            string lang = (languageParam.get() == 0) ? "ca" : "en";
            string style = (styleParam.get() == 0) ? "minimal" : "sagan";
            
            // Primer processem el tipus espectral
            SpectralTypeComponents spectralComponents = parseSpectralType(spectralTypeIn.get());
            //ofLogNotice("updateNarrative") << "Processing spectral type: " << spectralTypeIn.get();
            
            // Obtenim el tipus d'estel
            string starType = NarrativeManager::getInstance().getStarType(
                spectralComponents.classKey,
                spectralComponents.subKey,
                lang
            );
            //ofLogNotice("updateNarrative") << "Star type determined: " << starType;

            // Preparem tots els replacements
            map<string, string> replacements;
            replacements["STAR_NAME"] = processStarName(starNameIn.get(), lang);
            replacements["CONSTELLATION"] = constellationIn.get();
            replacements["COLOR"] = NarrativeManager::getInstance().getColorDescription(bvColorIn.get(), lang);
            replacements["DISTANCE"] = formatNumber(parallaxToLightYears(parallaxIn.get()), 1,lang);
            replacements["MULTIPLE_COUNT"] = ofToString(multipleCountIn.get());
            replacements["MAGNITUDE"] = formatNumber(magnitudeIn.get(), 2, lang);
            replacements["STAR_TYPE"] = starType;  // Usem el tipus que hem processat abans
            replacements["MASS"] = formatNumber(sunXIn.get(), 2,lang);

            // Debug dels valors
            for(const auto& [key, value] : replacements) {
                //ofLogNotice("updateNarrative") << "Replacement " << key << ": " << value;
            }

            // Obtenim i processem el template
            string template_text = NarrativeManager::getInstance().getNarrative(lang, style, true);
            if (!template_text.empty()) {
                string newNarrative = processTemplate(template_text, replacements);
                if (!newNarrative.empty()) {
                    //ofLogNotice("updateNarrative") << "Setting new narrative: " << newNarrative;
                    narrativeOut.set(newNarrative);
                }
            }
        }

        size_t findMatchingBrace(const string& text, size_t startPos) {
            int braceCount = 0;
            for (size_t pos = startPos; pos < text.length(); ++pos) {
                if (text[pos] == '{') {
                    braceCount++;
                } else if (text[pos] == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        return pos;
                    }
                }
            }
            return string::npos; // No matching brace found
        }
        
        string processStarName(const string& starName, const string& language) {
            // Check if it starts with "HD "
            if (starName.substr(0, 3) == "HD ") {
                string number = starName.substr(3); // Get the number part
                if (language == "ca") {
                    return "hac dé " + number;
                } else { // en
                    return "hache de " + number;
                }
            }
            return starName;
        }

        string processTemplate(string text, const map<string, string>& replacements) {
            size_t pos = 0;

            while ((pos = text.find("{", pos)) != string::npos) {
                size_t endPos = findMatchingBrace(text, pos);
                if (endPos == string::npos) break;

                string placeholder = text.substr(pos + 1, endPos - pos - 1);
                string replacement = "";

                // Processa condicions i placeholders
                if (placeholder.find(":") != string::npos) {
                    // És una condició
                    string condition = placeholder.substr(0, placeholder.find(":"));
                    string content = placeholder.substr(placeholder.find(":") + 1);

                    bool showContent = evaluateCondition(condition, replacements);
                    if (showContent) {
                        replacement = processTemplate(content, replacements); // Recursió
                    }
                } else if (replacements.find(placeholder) != replacements.end()) {
                    // Substitueix un placeholder simple
                    replacement = replacements.at(placeholder);
                } else {
                    // Placeholder no trobat
                    ofLogWarning("processTemplate") << "Replacement for {" << placeholder << "} was not found!";
                }

                text.replace(pos, endPos - pos + 1, replacement);
                // No incrementem pos, ja que el text pot canviar de mida
            }

            return text;
        }

        bool evaluateCondition(const string& condition, const map<string, string>& replacements) {
            try {
                string cond = condition;
                bool negate = false;

                // Comprova si la condició està negada
                if (cond[0] == '!') {
                    negate = true;
                    cond = cond.substr(1); // Elimina '!' de la condició
                }

                bool result = false;

                if (replacements.find(cond) != replacements.end()) {
                    // La condició és una clau simple
                    result = !replacements.at(cond).empty();
                } else if (cond.find(">") != string::npos) {
                    string key = cond.substr(0, cond.find(">"));
                    if (replacements.find(key) == replacements.end()) return false;
                    float value = ofToFloat(replacements.at(key));
                    float threshold = ofToFloat(cond.substr(cond.find(">") + 1));
                    result = value > threshold;
                } else if (cond.find("==") != string::npos) {
                    string key = cond.substr(0, cond.find("=="));
                    if (replacements.find(key) == replacements.end()) return false;
                    int value = ofToInt(replacements.at(key));
                    int target = ofToInt(cond.substr(cond.find("==") + 2));
                    result = value == target;
                } else {
                    // Si la condició no conté operadors, comprova si la clau existeix i no està buida
                    result = replacements.find(cond) != replacements.end() && !replacements.at(cond).empty();
                }

                // Aplica la negació si cal
                if (negate) {
                    result = !result;
                }

                return result;
            } catch (const std::exception& e) {
                ofLogError("evaluateCondition") << "Error evaluating condition: " << condition << ". Exception: " << e.what();
            }
            return false;
        }


        float parallaxToLightYears(float parallax) {
            if (parallax <= 0) return 0;
            float parsecs = 1.0f / parallax; // Paral·laxi en arcsegons
            return parsecs * 3.26156f; // Convertir parsecs a anys llum
        }



        string formatNumber(float number, int precision, const string& lang) {
            // Check if it's a magnitude that needs the minus/menys prefix
            bool isNegative = number < 0;
            string numStr = ofToString(abs(number), precision); // Use absolute value

            // Find the position of the decimal point
            size_t decimalPos = numStr.find('.');
            if (decimalPos != string::npos) {
                // If the first decimal after the point is a zero, eliminate the decimal
                if (numStr[decimalPos + 1] == '0') {
                    numStr = numStr.substr(0, decimalPos); // Return only the integer part
                } else if (precision > 1) {
                    // Keep only the first decimal and eliminate the restants
                    numStr = numStr.substr(0, decimalPos + 2);
                }
            }

            // Add language-specific prefix for negative numbers
            if (isNegative) {
                if (lang == "ca") {
                    numStr = "menys " + numStr;
                } else { // en
                    numStr = "minus " + numStr;
                }
            }

            // Replace decimal separator according to language
            if (lang == "ca") {
                ofStringReplace(numStr, ".", " coma ");
            } else { // en
                ofStringReplace(numStr, ".", " point ");
            }

            return numStr;
        }

        ofParameter<float> parallaxIn;
            ofParameter<float> magnitudeIn;
            ofParameter<float> bvColorIn;
            ofParameter<string> spectralTypeIn;
            ofParameter<int> multipleCountIn;
            ofParameter<string> starNameIn;
            ofParameter<string> constellationIn;
            ofParameter<float> sunXIn;
            ofParameter<int> languageParam;
            ofParameter<int> styleParam;
            ofParameter<string> narrativeOut;
            
            ofEventListeners listeners;
    };
