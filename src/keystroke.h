// keystroke.h
#pragma once
#include "ofxOceanodeNodeModel.h"
#include <string>
#include <sstream>

class keystroke : public ofxOceanodeNodeModel {
public:
	keystroke() : ofxOceanodeNodeModel("Keystroke") {}

	void setup() override {
		color = ofColor(255, 180, 0);
		description = "Fires a void trigger on 'Output' when the key pressed matches the 'Key' parameter. 'Last Key' outputs the name of the last pressed key. When 'Combo' is on, 'Key' accepts modifier+key combinations like cmd+space or ctrl+shift+a.";

		addParameter(key_Param.set("Key", "a"));
		addParameter(combo_Param.set("Combo", false));

		addOutputParameter(triggerOutput.set("Output"));
		addOutputParameter(lastKeyOutput.set("Last Key", ""));

		keyPressListener = ofEvents().keyPressed.newListener(this, &keystroke::onKeyPressed);
	}

private:
	// ---------- Parameters ----------
	ofParameter<std::string> key_Param;
	ofParameter<bool>        combo_Param;

	// ---------- Output ----------
	ofParameter<void>        triggerOutput;
	ofParameter<std::string> lastKeyOutput;

	// ---------- Listeners ----------
	ofEventListener keyPressListener;

	// ---------- Helpers ----------

	void onKeyPressed(ofKeyEventArgs & args){
		if(combo_Param.get()){
			lastKeyOutput.set(keyToComboString(args));
		} else {
			lastKeyOutput.set(keyToString(args));
		}

		const std::string target = key_Param.get();
		if(target.empty()) return;

		bool matched = false;

		if(combo_Param.get()){
			matched = matchCombo(args, target);
		} else {
			if(target.size() == 1){
				matched = (args.codepoint == (uint32_t)target[0]);
				if(!matched) matched = (args.key == (int)(unsigned char)target[0]);
			} else {
				int namedKey = resolveNamedKey(target);
				if(namedKey != -1) matched = (args.key == namedKey);
			}
		}

		if(matched){
			triggerOutput.trigger();
		}
	}

	// Match "mod1+mod2+key" combo against the current key event.
	bool matchCombo(const ofKeyEventArgs & args, const std::string & target){
		std::vector<std::string> parts = splitPlus(target);
		if(parts.empty()) return false;

		std::string baseKey = parts.back();

		// Resolve required modifiers
		bool needShift = false, needCtrl = false, needAlt = false, needSuper = false;
		for(int i = 0; i < (int)parts.size() - 1; i++){
			std::string m = toLower(parts[i]);
			if(m == "shift")                                    needShift = true;
			else if(m == "ctrl" || m == "control")              needCtrl  = true;
			else if(m == "alt"  || m == "option")               needAlt   = true;
			else if(m == "cmd"  || m == "command" || m == "super") needSuper = true;
		}

		// Check that required modifiers are held
		if(needShift && !ofGetKeyPressed(OF_KEY_SHIFT))   return false;
		if(needCtrl  && !ofGetKeyPressed(OF_KEY_CONTROL)) return false;
		if(needAlt   && !ofGetKeyPressed(OF_KEY_ALT))     return false;
		if(needSuper && !ofGetKeyPressed(OF_KEY_SUPER))   return false;

		// Check base key
		if(baseKey.size() == 1){
			if(args.codepoint == (uint32_t)baseKey[0]) return true;
			return (args.key == (int)(unsigned char)baseKey[0]);
		} else {
			int namedKey = resolveNamedKey(baseKey);
			if(namedKey != -1) return (args.key == namedKey);
		}
		return false;
	}

	// Build a combo string from the current event (for Last Key output in combo mode).
	static std::string keyToComboString(const ofKeyEventArgs & args){
		std::string result;
		if(ofGetKeyPressed(OF_KEY_SUPER))    result += "cmd+";
		if(ofGetKeyPressed(OF_KEY_CONTROL))  result += "ctrl+";
		if(ofGetKeyPressed(OF_KEY_ALT))      result += "alt+";
		if(ofGetKeyPressed(OF_KEY_SHIFT))    result += "shift+";

		std::string base = keyToString(args);
		if(base.empty()) return result; // modifier-only event
		result += base;
		return result;
	}

	// Convert a key event to a human-readable string.
	static std::string keyToString(const ofKeyEventArgs & args){
		if(args.codepoint >= 32 && args.codepoint < 127){
			return std::string(1, (char)args.codepoint);
		}
		switch(args.key){
			case OF_KEY_RETURN:    return "return";
			case OF_KEY_BACKSPACE: return "backspace";
			case OF_KEY_DEL:       return "delete";
			case OF_KEY_TAB:       return "tab";
			case OF_KEY_ESC:       return "esc";
			case OF_KEY_LEFT:      return "left";
			case OF_KEY_RIGHT:     return "right";
			case OF_KEY_UP:        return "up";
			case OF_KEY_DOWN:      return "down";
			case OF_KEY_PAGE_UP:   return "pageup";
			case OF_KEY_PAGE_DOWN: return "pagedown";
			case OF_KEY_HOME:      return "home";
			case OF_KEY_END:       return "end";
			case OF_KEY_INSERT:    return "insert";
			case OF_KEY_SHIFT:     return "shift";
			case OF_KEY_CONTROL:   return "ctrl";
			case OF_KEY_ALT:       return "alt";
			case OF_KEY_SUPER:     return "super";
			case OF_KEY_F1:        return "f1";
			case OF_KEY_F2:        return "f2";
			case OF_KEY_F3:        return "f3";
			case OF_KEY_F4:        return "f4";
			case OF_KEY_F5:        return "f5";
			case OF_KEY_F6:        return "f6";
			case OF_KEY_F7:        return "f7";
			case OF_KEY_F8:        return "f8";
			case OF_KEY_F9:        return "f9";
			case OF_KEY_F10:       return "f10";
			case OF_KEY_F11:       return "f11";
			case OF_KEY_F12:       return "f12";
			default:               return "";
		}
	}

	// Map common human-readable key names to OF key codes.
	static int resolveNamedKey(const std::string & name){
		auto eq = [&](const char * s){ return toLower(name) == toLower(s); };

		if(name == " ")                         return 32;
		if(eq("space"))                         return 32;
		if(eq("return") || eq("enter"))         return OF_KEY_RETURN;
		if(eq("backspace"))                     return OF_KEY_BACKSPACE;
		if(eq("delete") || eq("del"))           return OF_KEY_DEL;
		if(eq("tab"))                           return OF_KEY_TAB;
		if(eq("esc") || eq("escape"))           return OF_KEY_ESC;
		if(eq("left"))                          return OF_KEY_LEFT;
		if(eq("right"))                         return OF_KEY_RIGHT;
		if(eq("up"))                            return OF_KEY_UP;
		if(eq("down"))                          return OF_KEY_DOWN;
		if(eq("pageup"))                        return OF_KEY_PAGE_UP;
		if(eq("pagedown"))                      return OF_KEY_PAGE_DOWN;
		if(eq("home"))                          return OF_KEY_HOME;
		if(eq("end"))                           return OF_KEY_END;
		if(eq("insert"))                        return OF_KEY_INSERT;
		if(eq("shift"))                         return OF_KEY_SHIFT;
		if(eq("ctrl") || eq("control"))         return OF_KEY_CONTROL;
		if(eq("alt"))                           return OF_KEY_ALT;
		if(eq("super") || eq("cmd") || eq("command")) return OF_KEY_SUPER;
		if(eq("f1"))  return OF_KEY_F1;
		if(eq("f2"))  return OF_KEY_F2;
		if(eq("f3"))  return OF_KEY_F3;
		if(eq("f4"))  return OF_KEY_F4;
		if(eq("f5"))  return OF_KEY_F5;
		if(eq("f6"))  return OF_KEY_F6;
		if(eq("f7"))  return OF_KEY_F7;
		if(eq("f8"))  return OF_KEY_F8;
		if(eq("f9"))  return OF_KEY_F9;
		if(eq("f10")) return OF_KEY_F10;
		if(eq("f11")) return OF_KEY_F11;
		if(eq("f12")) return OF_KEY_F12;
		return -1;
	}

	// Split string by '+', trimming whitespace from each token.
	static std::vector<std::string> splitPlus(const std::string & s){
		std::vector<std::string> parts;
		std::stringstream ss(s);
		std::string token;
		while(std::getline(ss, token, '+')){
			// trim
			size_t start = token.find_first_not_of(" \t");
			size_t end   = token.find_last_not_of(" \t");
			if(start != std::string::npos) parts.push_back(token.substr(start, end - start + 1));
		}
		return parts;
	}

	static std::string toLower(const std::string & s){
		std::string r = s;
		std::transform(r.begin(), r.end(), r.begin(), ::tolower);
		return r;
	}
	static std::string toLower(const char * s){ return toLower(std::string(s)); }
};
