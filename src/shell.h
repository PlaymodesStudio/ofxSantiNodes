#ifndef shell_h
#define shell_h

#include "ofxOceanodeNodeModel.h"

class shell : public ofxOceanodeNodeModel {
public:
    shell() : ofxOceanodeNodeModel("Shell") {}

    void setup() {
        addParameter(command.set("Command", ""));
        addParameter(execButton.set("Exec"));

        listeners.push(execButton.newListener([this](){
            executeCommand();
        }));
    }

private:
    ofEventListeners listeners;
    ofParameter<string> command;
    ofParameter<void> execButton;

    void executeCommand() {
        if (!command.get().empty()) {
            string cmd = command.get();
            // Use popen to execute the command and capture the output
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                ofLogError("shell") << "popen() failed!";
                return;
            }
            
            char buffer[128];
            std::string result = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    result += buffer;
            }
            pclose(pipe);

            ofLogNotice("shell") << "Command executed: " << cmd;
            ofLogNotice("shell") << "Output: " << result;
        } else {
            ofLogWarning("shell") << "No command specified";
        }
    }
};

#endif /* shell_h */
