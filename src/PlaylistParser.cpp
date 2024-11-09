#include "PlaylistParser.h"
#include <algorithm>
#include <sstream>
#include <map>

PlaylistParser::PlaylistParser() {
    curl = curl_easy_init();
}

PlaylistParser::~PlaylistParser() {
    if(curl) {
        curl_easy_cleanup(curl);
    }
}

std::vector<PlaylistParser::StreamInfo> PlaylistParser::parse(const std::string& url) {
    std::vector<StreamInfo> streams;
    
    ofLogNotice("PlaylistParser") << "Parsing URL: " << url;
    
    // Download playlist content
    std::string content = downloadUrl(url);
    if(content.empty()) {
        ofLogError("PlaylistParser") << "Failed to download playlist: " << url;
        return streams;
    }
    
    ofLogNotice("PlaylistParser") << "Downloaded content length: " << content.length();
    ofLogNotice("PlaylistParser") << "Content preview: " << content.substr(0, 200);
    
    // Detect type and parse accordingly
    PlaylistType type = detectType(url, content);
    std::string baseUrl = getBaseUrl(url);
    
    ofLogNotice("PlaylistParser") << "Detected type: " <<
        (type == PlaylistType::M3U ? "M3U" :
         type == PlaylistType::M3U8 ? "M3U8" :
         type == PlaylistType::PLS ? "PLS" :
         type == PlaylistType::DIRECT_STREAM ? "DIRECT" : "UNKNOWN");
    
    switch(type) {
        case PlaylistType::M3U:
            streams = parseM3U(content);
            break;
        case PlaylistType::M3U8:
            streams = parseM3U8(content, baseUrl);
            break;
        case PlaylistType::PLS:
            streams = parsePLS(content);
            break;
        case PlaylistType::DIRECT_STREAM: {
            StreamInfo info;
            info.url = url;
            streams.push_back(info);
            break;
        }
        default:
            ofLogError("PlaylistParser") << "Unknown playlist type for: " << url;
    }
    
    ofLogNotice("PlaylistParser") << "Found " << streams.size() << " streams";
    for(size_t i = 0; i < streams.size(); i++) {
        ofLogNotice("PlaylistParser") << "Stream " << i << ": " << streams[i].url;
    }
    
    return streams;
}

PlaylistParser::PlaylistType PlaylistParser::detectType(const std::string& url, const std::string& content) {
    // Check URL extension first
    std::string lowerUrl = toLower(url);
    if(containsString(lowerUrl, ".m3u8")) return PlaylistType::M3U8;
    if(containsString(lowerUrl, ".m3u")) return PlaylistType::M3U;
    if(containsString(lowerUrl, ".pls")) return PlaylistType::PLS;
    
    // Check content
    std::string lowerContent = toLower(content);
    if(content.substr(0, 7) == "#EXTM3U") {
        return containsString(lowerContent, "#ext-x-stream-inf") ?
               PlaylistType::M3U8 : PlaylistType::M3U;
    }
    
    if(containsString(lowerContent, "[playlist]")) {
        return PlaylistType::PLS;
    }
    
    // Check if it's likely a direct stream
    if(containsString(lowerUrl, ".mp3") ||
       containsString(lowerUrl, ".aac") ||
       containsString(lowerUrl, "stream") ||
       containsString(lowerUrl, "listen")) {
        return PlaylistType::DIRECT_STREAM;
    }
    
    return PlaylistType::UNKNOWN;
}

std::vector<PlaylistParser::StreamInfo> PlaylistParser::parseM3U(const std::string& content) {
    std::vector<StreamInfo> streams;
    std::istringstream iss(content);
    std::string line;
    StreamInfo currentStream;
    
    while(std::getline(iss, line)) {
        // Trim whitespace
        line = trim(line);
        if(line.empty() || line[0] == '#') {
            // Parse extended info if present
            if(startsWith(line, "#EXTINF:")) {
                size_t titleStart = line.find(',');
                if(titleStart != std::string::npos) {
                    currentStream.title = line.substr(titleStart + 1);
                }
            }
            continue;
        }
        
        // Found a URL
        currentStream.url = line;
        streams.push_back(currentStream);
        currentStream = StreamInfo(); // Reset for next entry
    }
    
    return streams;
}

std::vector<PlaylistParser::StreamInfo> PlaylistParser::parseM3U8(const std::string& content,
                                                                 const std::string& baseUrl) {
    std::vector<StreamInfo> streams;
    std::istringstream iss(content);
    std::string line;
    StreamInfo currentStream;
    bool isVariantPlaylist = false;
    
    while(std::getline(iss, line)) {
        line = trim(line);
        if(line.empty()) continue;
        
        if(startsWith(line, "#EXT-X-STREAM-INF:")) {
            isVariantPlaylist = true;
            currentStream.isHLS = true;
            
            // Parse bandwidth if present
            size_t bwPos = line.find("BANDWIDTH=");
            if(bwPos != std::string::npos) {
                std::string bwStr = line.substr(bwPos + 10);
                size_t comma = bwStr.find(',');
                if(comma != std::string::npos) {
                    bwStr = bwStr.substr(0, comma);
                }
                currentStream.bandwidth = std::stoi(bwStr);
            }
        }
        else if(!line.empty() && line[0] != '#') {
            currentStream.url = resolveUrl(baseUrl, line);
            streams.push_back(currentStream);
            currentStream = StreamInfo();
        }
    }
    
    // If this isn't a variant playlist, treat it as a direct stream
    if(!isVariantPlaylist && streams.empty()) {
        StreamInfo info;
        info.url = baseUrl;
        info.isHLS = true;
        streams.push_back(info);
    }
    
    return streams;
}

std::vector<PlaylistParser::StreamInfo> PlaylistParser::parsePLS(const std::string& content) {
    std::vector<StreamInfo> streams;
    std::istringstream iss(content);
    std::string line;
    std::map<int, StreamInfo> streamMap;
    
    while(std::getline(iss, line)) {
        line = trim(line);
        if(line.empty() || line == "[playlist]") continue;
        
        // Parse key=value pairs
        size_t equals = line.find('=');
        if(equals == std::string::npos) continue;
        
        std::string key = toLower(line.substr(0, equals));
        std::string value = line.substr(equals + 1);
        
        // Extract entry number and handle file/title entries
        int index = 0;
        if(startsWith(key, "file")) {
            index = std::stoi(key.substr(4));
            streamMap[index].url = value;
        }
        else if(startsWith(key, "title")) {
            index = std::stoi(key.substr(5));
            streamMap[index].title = value;
        }
    }
    
    // Convert map to vector
    for(const auto& pair : streamMap) {
        if(!pair.second.url.empty()) {
            streams.push_back(pair.second);
        }
    }
    
    return streams;
}

size_t PlaylistParser::WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::string* buffer = static_cast<std::string*>(userdata);
    size_t totalSize = size * nmemb;
    buffer->append(ptr, totalSize);
    return totalSize;
}

std::string PlaylistParser::downloadUrl(const std::string& url) {
    if(!curl) return "";
    
    curl_easy_reset(curl);
    buffer.clear();
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    
    // Enable error buffer
    char errorBuffer[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    
    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        ofLogError("PlaylistParser") << "Download failed: " << curl_easy_strerror(res);
        if(errorBuffer[0] != '\0') {
            ofLogError("PlaylistParser") << "Error details: " << errorBuffer;
        }
        return "";
    }
    
    return buffer;
}

std::string PlaylistParser::getBaseUrl(const std::string& url) {
    size_t lastSlash = url.rfind('/');
    return lastSlash != std::string::npos ? url.substr(0, lastSlash + 1) : url;
}

std::string PlaylistParser::resolveUrl(const std::string& baseUrl, const std::string& relativeUrl) {
    if(startsWith(relativeUrl, "http://") || startsWith(relativeUrl, "https://")) {
        return relativeUrl;
    }
    return baseUrl + (relativeUrl[0] == '/' ? relativeUrl.substr(1) : relativeUrl);
}
