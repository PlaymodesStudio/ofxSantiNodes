#ifndef radioTuner_h
#define radioTuner_h

#include "ofxOceanodeNodeModel.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <curl/curl.h>
#include "ofThread.h"
#include <algorithm>
#include <string>
#include <vector>
#include "minimp3.h"
#include "minimp3_ex.h"

class StreamBuffer {
public:
    // Change these to enum or static constexpr
    enum {
        BUFFER_SIZE = 1024 * 1024 * 4,    // 4MB circular buffer
        MP3_BUFFER_SIZE = 16384,          // MP3 buffer for frame alignment
        PCM_BUFFER_SIZE = 44100 * 4       // 4 seconds of stereo audio at 44.1kHz
    };

    char buffer[BUFFER_SIZE];
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    std::atomic<bool> active{false};
    ofMutex mutex;
    
    // MP3 decoding state
    mp3dec_t mp3d;
    mp3dec_frame_info_t info;
    unsigned char mp3_buffer[MP3_BUFFER_SIZE];
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    
    // PCM ring buffer for decoded audio
    float pcm_buffer[PCM_BUFFER_SIZE * 2]; // *2 for stereo
    std::atomic<size_t> pcm_write_pos{0};
    std::atomic<size_t> pcm_read_pos{0};
    
    StreamBuffer() {
        mp3dec_init(&mp3d);
    }
    
    size_t write(const char* data, size_t len);
    void decodeSome();
    size_t readAndDecode(float* outBuffer, size_t numFrames);
    void clear();
    float getBufferLevel();
};

class radioTuner : public ofxOceanodeNodeModel, public ofThread {
public:
    struct AudioDeviceInfo {
        string name;
        AudioDeviceID deviceId;
        vector<UInt32> outputChannels;
    };
    
    radioTuner();
    ~radioTuner();
    
    void setup() override;

private:
    std::mutex urlMutex;
    std::atomic<bool> urlChanged{false};
    string safeUrl;
    
    // Parameters
    ofParameter<int> deviceSelector;
    ofParameter<bool> isPlaying{"Play", false};
    ofParameter<float> volume{"Volume", 1.0, 0.0, 1.0};
    ofParameter<int> stationSelector;
    
    // Listeners
    ofEventListeners listeners;
    
    // Data
    vector<string> stationNames;
    vector<string> stationUrls;
    vector<string> deviceNames;
    vector<AudioDeviceInfo> devices;
    AudioComponent audioComponent = nullptr;
    AudioComponentInstance audioUnit = nullptr;
    StreamBuffer streamBuffer;
    CURL* curl = nullptr;
    string currentUrl;
    std::atomic<bool> shouldStartStream{false};
    std::atomic<bool> shouldStopStream{false};
    
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static OSStatus audioCallback(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *ioData);
    
    void threadedFunction() override;
    void decodeAudioStream();
    bool parseStreamUrl(const string& url);
    bool setupAudioUnit();
    bool setupAudioOutputDevice(AudioDeviceID deviceId);  // Added this declaration
    void loadStations();
    void loadAudioDevices();
    void setupParameters();
    void startStream();
    void stopStream();
    
    friend class StreamBuffer; // Allow StreamBuffer to access private members
};

#endif /* radioTuner_h */
