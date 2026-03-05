// keystroke.h
#pragma once
#include "ofxOceanodeNodeModel.h"
#include <string>

class keystroke : public ofxOceanodeNodeModel {
public:
	keystroke() : ofxOceanodeNodeModel("Keystroke") {}

	void setup() override {
		color = ofColor(255, 180, 0);
		description = "Fires a void trigger on 'Output' when the key pressed matches the 'Key' parameter.";

		// A single-character (or named key) string the user wants to watch for.
		// Examples: "a", "A", " " (space), "F1", "return", "left", etc.
		addParameter(key_Param.set("Key", "a"));

		addOutputParameter(triggerOutput.set("Output"));

		// Listen for keypress events from openFrameworks
		keyPressListener = ofEvents().keyPressed.newListener(this, &keystroke::onKeyPressed);
	}

private:
	// ---------- Parameters ----------
	ofParameter<std::string> key_Param;

	// ---------- Output ----------
	ofParameter<void> triggerOutput;

	// ---------- Listeners ----------
	ofEventListener keyPressListener;

	// ---------- Helpers ----------

	// Resolve the target string into a key code / character for comparison.
	// Returns true and sets outChar/outKey when we have a match candidate.
	void onKeyPressed(ofKeyEventArgs & args){
		const std::string target = key_Param.get();
		if(target.empty()) return;

		bool matched = false;

		// ---- single printable character comparison ----
		if(target.size() == 1){
			// Compare against the unicode codepoint directly
			matched = (args.codepoint == (uint32_t)target[0]);
			// Fallback: compare against the raw key value (covers uppercase etc.)
			if(!matched){
				matched = (args.key == (int)(unsigned char)target[0]);
			}
		}
		// ---- named / special keys ----
		else {
			int namedKey = resolveNamedKey(target);
			if(namedKey != -1){
				matched = (args.key == namedKey);
			}
		}

		if(matched){
			triggerOutput.trigger();
		}
	}

	// Map common human-readable key names to OF key codes.
	static int resolveNamedKey(const std::string& name){
		// Case-insensitive compare helper
		auto eq = [&](const char* s){
			std::string n = name;
			std::string t = s;
			std::transform(n.begin(), n.end(), n.begin(), ::tolower);
			std::transform(t.begin(), t.end(), t.begin(), ::tolower);
			return n == t;
		};

		// Whitespace / editing
		if(eq("space") || eq(" "))   return OF_KEY_RETURN; // space bar: key == 32
		// OF uses 32 for space but let's be explicit:
		if(name == " ")              return 32;
		if(eq("space"))              return 32;
		if(eq("return") || eq("enter")) return OF_KEY_RETURN;
		if(eq("backspace"))          return OF_KEY_BACKSPACE;
		if(eq("delete") || eq("del")) return OF_KEY_DEL;
		if(eq("tab"))                return OF_KEY_TAB;
		if(eq("esc") || eq("escape")) return OF_KEY_ESC;

		// Arrow keys
		if(eq("left"))               return OF_KEY_LEFT;
		if(eq("right"))              return OF_KEY_RIGHT;
		if(eq("up"))                 return OF_KEY_UP;
		if(eq("down"))               return OF_KEY_DOWN;

		// Page / home / end
		if(eq("pageup"))             return OF_KEY_PAGE_UP;
		if(eq("pagedown"))           return OF_KEY_PAGE_DOWN;
		if(eq("home"))               return OF_KEY_HOME;
		if(eq("end"))                return OF_KEY_END;
		if(eq("insert"))             return OF_KEY_INSERT;

		// Modifier keys (useful for listening in isolation)
		if(eq("shift"))              return OF_KEY_SHIFT;
		if(eq("ctrl") || eq("control")) return OF_KEY_CONTROL;
		if(eq("alt"))                return OF_KEY_ALT;
		if(eq("super") || eq("cmd") || eq("command")) return OF_KEY_SUPER;

		// Function keys F1-F12
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

		return -1; // unknown
	}
};
