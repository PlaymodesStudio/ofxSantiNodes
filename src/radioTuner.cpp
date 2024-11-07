#define MINIMP3_IMPLEMENTATION
#include "radioTuner.h"
#include "ofJson.h"

size_t StreamBuffer::write(const char* data, size_t len) {
    ofScopedLock lock(mutex);
    if (!active) return 0;
    
    size_t written = 0;
    while(written < len) {
        size_t available = BUFFER_SIZE - writePos;
        size_t toWrite = std::min<size_t>(available, len - written);
        memcpy(buffer + writePos, data + written, toWrite);
        writePos = (writePos + toWrite) % BUFFER_SIZE;
        written += toWrite;
    }
    
    // Try to decode immediately to keep PCM buffer full
    decodeSome();
    
    return written;
}

void StreamBuffer::decodeSome() {
    size_t available = (writePos - readPos + BUFFER_SIZE) % BUFFER_SIZE;
    
    while(available >= MP3_BUFFER_SIZE/2) {
        // Copy to temp buffer for decoding
        size_t toCopy = std::min<size_t>(available, MP3_BUFFER_SIZE);
        if(readPos + toCopy <= BUFFER_SIZE) {
            memcpy(mp3_buffer, buffer + readPos, toCopy);
        } else {
            size_t firstPart = BUFFER_SIZE - readPos;
            memcpy(mp3_buffer, buffer + readPos, firstPart);
            memcpy(mp3_buffer + firstPart, buffer, toCopy - firstPart);
        }
        
        // Decode MP3 frame
        int samples = mp3dec_decode_frame(&mp3d, mp3_buffer, toCopy, pcm, &info);
        if(samples > 0) {
            // Write to PCM buffer
            size_t pcm_available = PCM_BUFFER_SIZE -
                ((pcm_write_pos - pcm_read_pos + PCM_BUFFER_SIZE) % PCM_BUFFER_SIZE);
            
            if(pcm_available >= (size_t)samples) {
                for(int i = 0; i < samples; i++) {
                    size_t write_idx = (pcm_write_pos + i) % PCM_BUFFER_SIZE;
                    pcm_buffer[write_idx * 2] = pcm[i * 2] / 32768.0f;     // Left
                    pcm_buffer[write_idx * 2 + 1] = pcm[i * 2 + 1] / 32768.0f; // Right
                }
                pcm_write_pos = (pcm_write_pos + samples) % PCM_BUFFER_SIZE;
                readPos = (readPos + info.frame_bytes) % BUFFER_SIZE;
            } else {
                // PCM buffer full, stop decoding for now
                break;
            }
        } else {
            // Skip bad frame
            readPos = (readPos + 1) % BUFFER_SIZE;
        }
        
        available = (writePos - readPos + BUFFER_SIZE) % BUFFER_SIZE;
    }
}

size_t StreamBuffer::readAndDecode(float* outBuffer, size_t numFrames) {
    ofScopedLock lock(mutex);
    if (!active) return 0;
    
    // Read directly from PCM buffer
    size_t frames_available = (pcm_write_pos - pcm_read_pos + PCM_BUFFER_SIZE) % PCM_BUFFER_SIZE;
    size_t frames_to_read = std::min<size_t>(numFrames, frames_available);
    
    for(size_t i = 0; i < frames_to_read; i++) {
        size_t read_idx = (pcm_read_pos + i) % PCM_BUFFER_SIZE;
        outBuffer[i * 2] = pcm_buffer[read_idx * 2];         // Left
        outBuffer[i * 2 + 1] = pcm_buffer[read_idx * 2 + 1]; // Right
    }
    
    pcm_read_pos = (pcm_read_pos + frames_to_read) % PCM_BUFFER_SIZE;
    
    // If we're running low on decoded audio, trigger more decoding
    if(frames_available < PCM_BUFFER_SIZE/2) {
        decodeSome();
    }
    
    return frames_to_read;
}

void StreamBuffer::clear() {
    ofScopedLock lock(mutex);
    readPos = writePos = 0;
    pcm_read_pos = pcm_write_pos = 0;
    mp3dec_init(&mp3d);
}

float StreamBuffer::getBufferLevel() {
    size_t frames_available = (pcm_write_pos - pcm_read_pos + PCM_BUFFER_SIZE) % PCM_BUFFER_SIZE;
    return (float)frames_available / PCM_BUFFER_SIZE;
}



///
///
///
///

radioTuner::radioTuner() : ofxOceanodeNodeModel("Radio Tuner"), ofThread() {
    // Initialize curl
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Initialize audio unit
    if(!setupAudioUnit()) {
        ofLogError("radioTuner") << "Failed to initialize audio unit in constructor";
    } else {
        ofLogNotice("radioTuner") << "Audio unit initialized successfully";
    }
    
    loadStations();
    loadAudioDevices();
    setupParameters();
    
    startThread(true);
}

radioTuner::~radioTuner() {
    if(isThreadRunning()) {
        shouldStopStream = true;
        stopThread();
        waitForThread(true);
    }
    
    if(audioUnit) {
        AudioOutputUnitStop(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
    }
    
    if(curl) {
        curl_easy_cleanup(curl);
        curl = nullptr;
    }
    
    curl_global_cleanup();
}

void radioTuner::setup() {
    description = "Internet radio tuner with multi-channel output routing";
}

size_t radioTuner::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    StreamBuffer* buffer = static_cast<StreamBuffer*>(userdata);
    size_t bytes = size * nmemb;
    return buffer->write(ptr, bytes);
}

OSStatus radioTuner::audioCallback(void *inRefCon,
                             AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames,
                             AudioBufferList *ioData) {
    radioTuner* tuner = static_cast<radioTuner*>(inRefCon);
    
    // Get buffer pointers for left and right channels
    float* leftBuffer = static_cast<float*>(ioData->mBuffers[0].mData);
    float* rightBuffer = ioData->mNumberBuffers > 1 ?
                        static_cast<float*>(ioData->mBuffers[1].mData) :
                        leftBuffer + 1;
    
    size_t stride = ioData->mNumberBuffers > 1 ? 1 : 2;
    
    // Temporary buffer for decoded audio
    float decodedBuffer[2048 * 2];  // Stereo buffer
    
    // Read and decode from the stream buffer
    size_t framesDecoded = tuner->streamBuffer.readAndDecode(decodedBuffer, inNumberFrames);
    
    // Get buffer level for adaptive handling
    float bufferLevel = tuner->streamBuffer.getBufferLevel();
    
    // Copy decoded frames to output
    for(UInt32 i = 0; i < inNumberFrames; i++) {
        if(i < framesDecoded) {
            // Apply subtle fade in/out based on buffer level to prevent clicks
            float fade = std::min(1.0f, std::min(bufferLevel * 2.0f, (1.0f - bufferLevel) * 2.0f));
            
            leftBuffer[i * stride] = decodedBuffer[i * 2] * tuner->volume * fade;
            rightBuffer[i * stride] = decodedBuffer[i * 2 + 1] * tuner->volume * fade;
        } else {
            // Buffer underrun - crossfade to silence
            float fade = std::max(0.0f, 1.0f - (float)(i - framesDecoded) / 32.0f);
            leftBuffer[i * stride] = 0.0f * fade;
            rightBuffer[i * stride] = 0.0f * fade;
        }
    }
    
    // Log if we're consistently running low on buffer
    static int underrunCount = 0;
    if(bufferLevel < 0.2) {
        if(++underrunCount > 100) {
            ofLogWarning("radioTuner") << "Low buffer level: " << bufferLevel;
            underrunCount = 0;
        }
    } else {
        underrunCount = 0;
    }
    
    return noErr;
}

void radioTuner::threadedFunction() {
    while(isThreadRunning()) {
        try {
            if(shouldStartStream || urlChanged) {
                string streamUrl;
                {
                    std::lock_guard<std::mutex> lock(urlMutex);
                    streamUrl = safeUrl;
                    urlChanged = false;
                }
                
                if(!streamUrl.empty()) {
                    // Clean up existing connection
                    if(curl) {
                        curl_easy_cleanup(curl);
                        curl = nullptr;
                    }
                    
                    // Create new connection
                    curl = curl_easy_init();
                    if(curl && parseStreamUrl(streamUrl)) {
                        streamBuffer.clear();
                        streamBuffer.active = true;
                        decodeAudioStream();
                    }
                }
                shouldStartStream = false;
            }
            
            if(shouldStopStream) {
                streamBuffer.active = false;
                if(curl) {
                    curl_easy_cleanup(curl);
                    curl = nullptr;
                }
                shouldStopStream = false;
            }
            
            ofSleepMillis(100);
        } catch (const std::exception& e) {
            ofLogError("radioTuner") << "Exception in threadedFunction: " << e.what();
        }
    }
}

void radioTuner::decodeAudioStream() {
    ofLogNotice("radioTuner") << "Starting decode stream...";
    
    if(!curl) {
        ofLogError("radioTuner") << "No CURL connection available";
        return;
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    if(res != CURLE_OK) {
        ofLogError("radioTuner") << "Stream error: " << curl_easy_strerror(res);
    }
    
    ofLogNotice("radioTuner") << "Decode stream ended";
}

bool radioTuner::parseStreamUrl(const string& url) {
    if(url.empty() || !curl) return false;
    
    try {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RadioTuner/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16384L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
        
        return true;
    } catch(const std::exception& e) {
        ofLogError("radioTuner") << "URL parsing error: " << e.what();
        return false;
    }
}

bool radioTuner::setupAudioUnit() {
    if(audioUnit != nullptr) {
        ofLogNotice("radioTuner") << "Audio unit already initialized";
        return true;
    }

    // Describe audio component
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    #if TARGET_OS_IPHONE
        desc.componentSubType = kAudioUnitSubType_RemoteIO;
    #else
        desc.componentSubType = kAudioUnitSubType_HALOutput;
    #endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    // Get component
    audioComponent = AudioComponentFindNext(NULL, &desc);
    if(!audioComponent) {
        ofLogError("radioTuner") << "Failed to find audio component";
        return false;
    }
    
    // Create audio unit instance
    OSStatus status = AudioComponentInstanceNew(audioComponent, &audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to create audio unit";
        return false;
    }
    
    // Set up the audio format
    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate = 44100;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = 2;  // Stereo
    audioFormat.mBitsPerChannel = 32;
    audioFormat.mBytesPerPacket = audioFormat.mBitsPerChannel / 8;
    audioFormat.mBytesPerFrame = audioFormat.mBytesPerPacket;
    
    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &audioFormat,
                                sizeof(audioFormat));
    
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to set audio format";
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return false;
    }
    
    // Set callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = audioCallback;
    callbackStruct.inputProcRefCon = this;
    
    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &callbackStruct,
                                sizeof(callbackStruct));
    
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to set audio callback";
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return false;
    }
    
    // Initialize
    status = AudioUnitInitialize(audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to initialize audio unit";
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return false;
    }
    
    ofLogNotice("radioTuner") << "Audio unit setup completed successfully";
    return true;
}

void radioTuner::loadStations() {
    string path = ofToDataPath("radio/stations.json");
    if(!ofFile::doesFileExist(path)) {
        ofLogError("radioTuner") << "stations.json not found at: " << path;
        return;
    }
    
    try {
        ofJson json = ofLoadJson(path);
        stationNames.clear();
        stationUrls.clear();
        
        // Sort keys for consistent order
        vector<string> keys;
        for(auto& [key, value] : json.items()) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        
        for(const auto& key : keys) {
            stationNames.push_back(key);
            stationUrls.push_back(json[key]);
        }
        
        ofLogNotice("radioTuner") << "Loaded " << stationNames.size() << " stations";
    }
    catch(const std::exception& e) {
        ofLogError("radioTuner") << "Error loading stations: " << e.what();
    }
}

void radioTuner::loadAudioDevices() {
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                   &propertyAddress,
                                                   0,
                                                   NULL,
                                                   &dataSize);
    
    if(status != noErr) {
        ofLogError("radioTuner") << "Error getting audio devices size";
        return;
    }
    
    int deviceCount = dataSize / sizeof(AudioDeviceID);
    vector<AudioDeviceID> deviceIds(deviceCount);
    
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &propertyAddress,
                                      0,
                                      NULL,
                                      &dataSize,
                                      deviceIds.data());
    
    if(status != noErr) {
        ofLogError("radioTuner") << "Error getting audio devices";
        return;
    }
    
    deviceNames.clear();
    devices.clear();
    
    // Add system default device first
    deviceNames.push_back("System Default");
    AudioDeviceInfo defaultDevice;
    defaultDevice.name = "System Default";
        defaultDevice.deviceId = kAudioObjectSystemObject;
        devices.push_back(defaultDevice);
        
        for(const auto& deviceId : deviceIds) {
            AudioDeviceInfo info;
            info.deviceId = deviceId;
            
            // Get device name
            CFStringRef deviceName;
            dataSize = sizeof(CFStringRef);
            propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
            
            status = AudioObjectGetPropertyData(deviceId,
                                              &propertyAddress,
                                              0,
                                              NULL,
                                              &dataSize,
                                              &deviceName);
                                              
            if(status == noErr) {
                char name[256];
                CFStringGetCString(deviceName, name, 256, kCFStringEncodingUTF8);
                CFRelease(deviceName);
                
                info.name = string(name);
                deviceNames.push_back(info.name);
                
                // Get output channels
                propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
                propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
                
                status = AudioObjectGetPropertyDataSize(deviceId,
                                                      &propertyAddress,
                                                      0,
                                                      NULL,
                                                      &dataSize);
                                                      
                if(status == noErr) {
                    AudioBufferList* bufferList = (AudioBufferList*)malloc(dataSize);
                    status = AudioObjectGetPropertyData(deviceId,
                                                      &propertyAddress,
                                                      0,
                                                      NULL,
                                                      &dataSize,
                                                      bufferList);
                                                      
                    if(status == noErr) {
                        for(UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
                            for(UInt32 ch = 0; ch < bufferList->mBuffers[i].mNumberChannels; ch++) {
                                info.outputChannels.push_back(ch);
                            }
                        }
                    }
                    
                    free(bufferList);
                }
                
                devices.push_back(info);
            }
        }
        
        ofLogNotice("radioTuner") << "Loaded " << deviceNames.size() << " audio devices";
    }

    void radioTuner::setupParameters() {
        // Setup dropdowns
        addParameterDropdown(stationSelector, "Station", 0, stationNames);
        addParameterDropdown(deviceSelector, "Audio Device", 0, deviceNames);
        
        // Add other parameters
        addParameter(isPlaying);
        addParameter(volume);
        
        // Add listeners
        listeners.push(isPlaying.newListener([this](bool& value) {
            if(value) {
                startStream();
            } else {
                stopStream();
            }
        }));
        
        listeners.push(stationSelector.newListener([this](int& value) {
            try {
                if(value >= 0 && value < stationUrls.size()) {
                    std::lock_guard<std::mutex> lock(urlMutex);
                    safeUrl = stationUrls[value];
                    if(isPlaying) {
                        stopStream();
                        startStream();
                    }
                }
            } catch (const std::exception& e) {
                ofLogError("radioTuner") << "Exception in station selector: " << e.what();
            }
        }));
        
        listeners.push(deviceSelector.newListener([this](int& value) {
            if(value >= 0 && value < devices.size()) {
                setupAudioOutputDevice(devices[value].deviceId);
            }
        }));
    }

    bool radioTuner::setupAudioOutputDevice(AudioDeviceID deviceId) {
        if(!audioUnit) {
            ofLogError("radioTuner") << "Cannot set device - audio unit not initialized";
            return false;
        }
        
        ofLogNotice("radioTuner") << "Setting audio device: " << deviceId;
        
        // Don't change device if it's system default
        if(deviceId == kAudioObjectSystemObject) {
            ofLogNotice("radioTuner") << "Using system default device";
            return true;
        }
        
        OSStatus status = AudioUnitSetProperty(audioUnit,
                                             kAudioOutputUnitProperty_CurrentDevice,
                                             kAudioUnitScope_Global,
                                             0,
                                             &deviceId,
                                             sizeof(deviceId));
                                             
        if(status != noErr) {
            ofLogError("radioTuner") << "Failed to set audio device: " << status;
            return false;
        }
        
        ofLogNotice("radioTuner") << "Successfully set audio device";
        return true;
    }

    void radioTuner::startStream() {
        try {
            string streamUrl;
            {
                std::lock_guard<std::mutex> lock(urlMutex);
                if(currentUrl.empty() && stationSelector >= 0 && stationSelector < stationUrls.size()) {
                    safeUrl = stationUrls[stationSelector];
                } else {
                    safeUrl = currentUrl;
                }
                streamUrl = safeUrl;
            }
            
            if(!streamUrl.empty()) {
                if(!audioUnit && !setupAudioUnit()) {
                    ofLogError("radioTuner") << "Failed to setup audio unit";
                    isPlaying = false;
                    return;
                }
                
                OSStatus status = AudioOutputUnitStart(audioUnit);
                if(status != noErr) {
                    ofLogError("radioTuner") << "Failed to start audio unit";
                    isPlaying = false;
                    return;
                }
                
                {
                    std::lock_guard<std::mutex> lock(urlMutex);
                    currentUrl = streamUrl;
                }
                shouldStartStream = true;
                urlChanged = true;
                ofLogNotice("radioTuner") << "Starting stream: " << streamUrl;
            } else {
                ofLogError("radioTuner") << "No URL selected";
                isPlaying = false;
            }
        } catch (const std::exception& e) {
            ofLogError("radioTuner") << "Exception in startStream: " << e.what();
            isPlaying = false;
        }
    }

    void radioTuner::stopStream() {
        if(audioUnit) {
            AudioOutputUnitStop(audioUnit);
        }
        shouldStopStream = true;
        ofLogNotice("radioTuner") << "Stream stopped";
    }
