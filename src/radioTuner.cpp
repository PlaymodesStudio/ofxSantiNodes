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
    // Ensure proper cleanup order
    stopStream();
    
    if(isThreadRunning()) {
        shouldStopStream = true;
        waitForThread(true);
    }
    
    // Stop audio before disposing
    if(audioUnit) {
        AudioOutputUnitStop(audioUnit);
        AudioUnitUninitialize(audioUnit);
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
    
    // Skip processing if device is changing
    if(tuner->isChangingDevice) {
        for(UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
            float* buffer = static_cast<float*>(ioData->mBuffers[i].mData);
            memset(buffer, 0, sizeof(float) * inNumberFrames);
        }
        return noErr;
    }

    // Clear all channels first
    for(UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
        float* buffer = static_cast<float*>(ioData->mBuffers[i].mData);
        memset(buffer, 0, sizeof(float) * inNumberFrames);
    }

    // Calculate target channels (0-based)
    int leftChannelIdx = tuner->channelSelector - 1;
    int rightChannelIdx = leftChannelIdx + 1;

    // Validate target channels
    if(leftChannelIdx < 0 ||
       rightChannelIdx >= (int)ioData->mNumberBuffers ||
       leftChannelIdx >= (int)ioData->mNumberBuffers) {
        ofLogError("radioTuner") << "Invalid channel routing: L=" << leftChannelIdx
                                << " R=" << rightChannelIdx
                                << " Max=" << ioData->mNumberBuffers;
        return noErr;
    }

    // Get pointers to target channel buffers
    float* leftBuffer = (float*)ioData->mBuffers[leftChannelIdx].mData;
    float* rightBuffer = (float*)ioData->mBuffers[rightChannelIdx].mData;

    if(!leftBuffer || !rightBuffer) {
        ofLogError("radioTuner") << "Null buffer pointers";
        return noErr;
    }

    // Temporary buffer for decoded audio
    float decodedBuffer[2048 * 2];  // Stereo buffer
    size_t framesDecoded = tuner->streamBuffer.readAndDecode(decodedBuffer, inNumberFrames);

    static int logCounter = 0;
    if(++logCounter >= 100) {
        ofLogNotice("radioTuner") << "Routing decoded audio:"
                                 << " Frames=" << framesDecoded
                                 << " To channels " << (leftChannelIdx + 1)
                                 << "," << (rightChannelIdx + 1)
                                 << " Buffer level=" << tuner->streamBuffer.getBufferLevel();
        logCounter = 0;
    }

    // Route decoded stereo audio to target channels
    float volume = tuner->volume;
    for(UInt32 i = 0; i < inNumberFrames; i++) {
        if(i < framesDecoded) {
            leftBuffer[i] = decodedBuffer[i * 2] * volume;
            rightBuffer[i] = decodedBuffer[i * 2 + 1] * volume;
        }
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

void radioTuner::cleanupAudioUnit() {
    std::lock_guard<std::mutex> lock(audioMutex);
    if(audioUnit) {
        AudioOutputUnitStop(audioUnit);
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
    }
}

bool radioTuner::recreateAudioUnit() {
    try {
        ofLogNotice("radioTuner") << "Starting audio unit recreation";
        cleanupAudioUnit();
        
        AudioComponentDescription desc;
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;
        
        AudioComponent component = AudioComponentFindNext(NULL, &desc);
        if(!component) {
            ofLogError("radioTuner") << "Failed to find audio component";
            return false;
        }
        
        OSStatus status = AudioComponentInstanceNew(component, &audioUnit);
        if(status != noErr) {
            ofLogError("radioTuner") << "Failed to create audio unit";
            return false;
        }
        
        // Set the device
        if(deviceSelector >= 0 && deviceSelector < devices.size()) {
            AudioDeviceID deviceId = devices[deviceSelector].deviceId;
            ofLogNotice("radioTuner") << "Setting up device: " << devices[deviceSelector].name
                                     << " (ID: " << deviceId << ")";
            
            // First get the device's native format
            AudioObjectPropertyAddress propertyAddress = {
                kAudioDevicePropertyStreamFormat,
                kAudioDevicePropertyScopeOutput,
                0
            };
            
            AudioStreamBasicDescription deviceFormat;
            UInt32 size = sizeof(deviceFormat);
            status = AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, &deviceFormat);
            
            ofLogNotice("radioTuner") << "\nDevice Native Format:";
            ofLogNotice("radioTuner") << "Sample Rate: " << deviceFormat.mSampleRate;
            ofLogNotice("radioTuner") << "Format ID: " << deviceFormat.mFormatID;
            ofLogNotice("radioTuner") << "Format Flags: " << deviceFormat.mFormatFlags;
            ofLogNotice("radioTuner") << "Bytes Per Packet: " << deviceFormat.mBytesPerPacket;
            ofLogNotice("radioTuner") << "Frames Per Packet: " << deviceFormat.mFramesPerPacket;
            ofLogNotice("radioTuner") << "Bytes Per Frame: " << deviceFormat.mBytesPerFrame;
            ofLogNotice("radioTuner") << "Channels Per Frame: " << deviceFormat.mChannelsPerFrame;
            ofLogNotice("radioTuner") << "Bits Per Channel: " << deviceFormat.mBitsPerChannel;
            
            // Now get the available physical channels
            propertyAddress.mSelector = kAudioDevicePropertyPreferredChannelLayout;
            AudioChannelLayout *layout = NULL;
            size = 0;
            
            status = AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 0, NULL, &size);
            if(status == noErr && size > 0) {
                layout = (AudioChannelLayout*)malloc(size);
                status = AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, layout);
                
                if(status == noErr) {
                    ofLogNotice("radioTuner") << "\nChannel Layout:";
                    ofLogNotice("radioTuner") << "Channel Layout Tag: " << layout->mChannelLayoutTag;
                    ofLogNotice("radioTuner") << "Channel Bitmap: " << layout->mChannelBitmap;
                    ofLogNotice("radioTuner") << "Number of Channels: " << layout->mNumberChannelDescriptions;
                    
                    for(UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++) {
                        ofLogNotice("radioTuner") << "Channel " << i << " Label: "
                                                 << layout->mChannelDescriptions[i].mChannelLabel;
                    }
                }
                free(layout);
            }
            
            // Set device
            status = AudioUnitSetProperty(audioUnit,
                                        kAudioOutputUnitProperty_CurrentDevice,
                                        kAudioUnitScope_Global,
                                        0,
                                        &deviceId,
                                        sizeof(deviceId));
            
            if(status != noErr) {
                ofLogError("radioTuner") << "Failed to set audio device";
                cleanupAudioUnit();
                return false;
            }
            
            // Get the device's stream configuration
            propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
            size = 0;
            status = AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 0, NULL, &size);
            if(status != noErr) {
                ofLogError("radioTuner") << "Failed to get stream configuration size";
                return false;
            }
            
            AudioBufferList* bufferList = (AudioBufferList*)malloc(size);
            status = AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, bufferList);
            
            UInt32 totalChannels = 0;
            if(status == noErr) {
                ofLogNotice("radioTuner") << "\nStream Configuration:";
                ofLogNotice("radioTuner") << "Number of buffer structs: " << bufferList->mNumberBuffers;
                
                for(UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
                    totalChannels += bufferList->mBuffers[i].mNumberChannels;
                    ofLogNotice("radioTuner") << "Buffer " << i << ": "
                                             << bufferList->mBuffers[i].mNumberChannels << " channels";
                }
            }
            free(bufferList);
            
            // Set an explicit format
            AudioStreamBasicDescription audioFormat;
            audioFormat.mSampleRate = deviceFormat.mSampleRate;
            audioFormat.mFormatID = kAudioFormatLinearPCM;
            audioFormat.mFormatFlags = kAudioFormatFlagIsFloat |
                                     kAudioFormatFlagIsPacked |
                                     kAudioFormatFlagsNativeEndian |
                                     kAudioFormatFlagIsNonInterleaved;
            audioFormat.mFramesPerPacket = 1;
            audioFormat.mChannelsPerFrame = totalChannels;  // Use total channels found
            audioFormat.mBitsPerChannel = 32;
            audioFormat.mBytesPerPacket = 4;
            audioFormat.mBytesPerFrame = 4;
            
            ofLogNotice("radioTuner") << "\nSetting Audio Format:";
            ofLogNotice("radioTuner") << "Sample Rate: " << audioFormat.mSampleRate;
            ofLogNotice("radioTuner") << "Channels: " << audioFormat.mChannelsPerFrame;
            ofLogNotice("radioTuner") << "Format Flags: 0x" << std::hex << audioFormat.mFormatFlags;
            
            // Try to set format
            status = AudioUnitSetProperty(audioUnit,
                                        kAudioUnitProperty_StreamFormat,
                                        kAudioUnitScope_Input,
                                        0,
                                        &audioFormat,
                                        sizeof(audioFormat));
            
            if(status != noErr) {
                ofLogError("radioTuner") << "Failed to set audio format. Status: " << status;
                // Try to get the actual format being used
                AudioStreamBasicDescription actualFormat;
                size = sizeof(actualFormat);
                status = AudioUnitGetProperty(audioUnit,
                                            kAudioUnitProperty_StreamFormat,
                                            kAudioUnitScope_Input,
                                            0,
                                            &actualFormat,
                                            &size);
                if(status == noErr) {
                    ofLogNotice("radioTuner") << "\nActual Format Being Used:";
                    ofLogNotice("radioTuner") << "Sample Rate: " << actualFormat.mSampleRate;
                    ofLogNotice("radioTuner") << "Channels: " << actualFormat.mChannelsPerFrame;
                    ofLogNotice("radioTuner") << "Format Flags: 0x" << std::hex << actualFormat.mFormatFlags;
                }
            }
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
            cleanupAudioUnit();
            return false;
        }
        
        // Initialize
        status = AudioUnitInitialize(audioUnit);
        if(status != noErr) {
            ofLogError("radioTuner") << "Failed to initialize audio unit";
            cleanupAudioUnit();
            return false;
        }
        
        // Start the audio unit
        status = AudioOutputUnitStart(audioUnit);
        if(status != noErr) {
            ofLogError("radioTuner") << "Failed to start audio unit";
            cleanupAudioUnit();
            return false;
        }
        
        ofLogNotice("radioTuner") << "Audio unit successfully created and started";
        return true;
        
    } catch (const std::exception& e) {
        ofLogError("radioTuner") << "Exception in recreateAudioUnit: " << e.what();
        return false;
    }
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
    addParameterDropdown(stationSelector, "Station", 0, stationNames);
    addParameterDropdown(deviceSelector, "Audio Device", 0, deviceNames);
    addParameter(channelSelector);
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
                // Set flag to prevent audio callback from processing
                isChangingDevice = true;
                
                // Stop playback if needed
                bool wasPlaying = isPlaying;
                if(wasPlaying) {
                    isPlaying = false;
                    stopStream();
                }
                
                // Change device
                if(recreateAudioUnit()) {
                    updateChannelCount();
                    
                    // Restore playback if needed
                    if(wasPlaying) {
                        isPlaying = true;
                        startStream();
                    }
                } else {
                    ofLogError("radioTuner") << "Failed to switch audio device";
                }
                
                isChangingDevice = false;
            }
        }));
    
    listeners.push(channelSelector.newListener([this](int& value) {
            ofLogNotice("radioTuner") << "Channel changed to: " << value;
            
            // Validate channel selection
            if(deviceSelector >= 0 && deviceSelector < devices.size()) {
                const auto& device = devices[deviceSelector];
                int maxChannels = device.outputChannels.size();
                
                if(value > maxChannels - 1) {
                    ofLogWarning("radioTuner") << "Selected channel " << value
                                             << " exceeds device channel count " << maxChannels;
                    value = 1; // Reset to first channel
                }
            }
        }));
    
}

void radioTuner::updateChannelCount() {
    if(deviceSelector < 0 || deviceSelector >= devices.size()) return;
    
    const auto& device = devices[deviceSelector];
    AudioDeviceID deviceId = device.deviceId;
    
    // Get the actual channel count from the device
    AudioObjectPropertyAddress propertyAddress = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        0
    };
    
    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 0, NULL, &dataSize);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to get stream configuration size";
        return;
    }
    
    AudioBufferList* bufferList = (AudioBufferList*)malloc(dataSize);
    status = AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &dataSize, bufferList);
    
    if(status == noErr) {
        // Count total channels across all buffers
        UInt32 totalChannels = 0;
        for(UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
            totalChannels += bufferList->mBuffers[i].mNumberChannels;
        }
        
        int maxChannels = totalChannels;
        if(maxChannels < 2) maxChannels = 2;
        
        // Calculate maximum starting channel (ensuring space for stereo pair)
        int maxStartChannel = maxChannels - 1;
        
        // Store current channel
        int currentChannel = channelSelector.get();
        
        // Update channel selector range
        channelSelector.setMax(maxStartChannel);
        
        ofLogNotice("radioTuner") << "Device: " << device.name
                                 << " Total channels: " << maxChannels
                                 << " Max start channel: " << maxStartChannel
                                 << " Current channel: " << currentChannel;
        
        // If current channel is out of range, adjust it
        if(currentChannel > maxStartChannel) {
            channelSelector = 1;
            ofLogNotice("radioTuner") << "Adjusted channel to 1 (was " << currentChannel << ")";
        }
    }
    
    free(bufferList);
}

bool radioTuner::setupAudioOutputDevice(AudioDeviceID deviceId) {
    std::lock_guard<std::mutex> lock(audioMutex);
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
    
    OSStatus status = AudioOutputUnitStop(audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to stop audio unit";
    }
    
    status = AudioUnitUninitialize(audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to uninitialize audio unit";
    }
    
    status = AudioUnitSetProperty(audioUnit,
                                kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global,
                                0,
                                &deviceId,
                                sizeof(deviceId));
                                
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to set audio device: " << status;
        return false;
    }
    
    status = AudioUnitInitialize(audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to initialize audio unit after device change";
        return false;
    }
    
    status = AudioOutputUnitStart(audioUnit);
    if(status != noErr) {
        ofLogError("radioTuner") << "Failed to start audio unit after device change";
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
    shouldStopStream = true;
    streamBuffer.active = false;
    
    std::lock_guard<std::mutex> lock(audioMutex);
    if(audioUnit) {
        AudioOutputUnitStop(audioUnit);
    }
    
    ofLogNotice("radioTuner") << "Stream stopped";
}
