#ifndef DATABUFFER_H
#define DATABUFFER_H

#include "ofxOceanodeNodeModel.h"

class dataBuffer : public ofxOceanodeNodeModel{
public:
    dataBuffer() : ofxOceanodeNodeModel("Data Buffer"){}

    void setup(){
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(delayFrames.set("Frames", {0}, {0}, {INT_MAX}));
        addParameter(bufferSize.set("Buffer Size", 10, 1, INT_MAX)); // New parameter
        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        bufferSizeListener = bufferSize.newListener([this](int &sz){
            // Resize the buffer to new size, handling overflow or expansion
            while(store.size() > sz){
                store.pop_front();
            }
        });
    }

    void update(ofEventArgs &a)
    {
        store.push_back(input.get());

        // Maintaining the buffer size
        while(store.size() > bufferSize){
            store.pop_front();
        }

        vector<int> newDelayFrames = delayFrames.get(); // Create a copy of the current delay frames

        // Ensure newDelayFrames has at least the same number of elements as input
        while(newDelayFrames.size() < input.get().size())
        {
            int lastFrame = newDelayFrames.back();
            newDelayFrames.push_back(lastFrame); // Modify the copy
        }

        vector<float> outAux;
        for(int i = 0; i < input.get().size(); i++)
        {
            int storeSize = store.size();
            int delayFrameI = newDelayFrames[i]; // Use the modified copy
            int pos = storeSize - delayFrameI - 1;
            float f = 0;
            if(pos >= 0 && pos < storeSize)
            {
                // Valid position in the buffer
                f = store[pos][i];
            }
            else
            {
                // Closest valid position in the buffer
                int closestPos = std::max(0, std::min(pos, storeSize - 1));
                if (!store.empty())
                {
                    f = store[closestPos][i];
                }
            }
            outAux.push_back(f);
        }
        output = outAux;
    }


private:
    ofEventListener bufferSizeListener;
    ofEventListener delayFramesListener;

    ofParameter<vector<float>> input;
    ofParameter<vector<int>> delayFrames;
    ofParameter<int> bufferSize; // New parameter
    ofParameter<vector<float>> output;
    deque<vector<float>> store;
};
#endif /* DATABUFFER_H */
