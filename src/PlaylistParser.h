// PlaylistParser.h
#ifndef PLAYLIST_PARSER_H
#define PLAYLIST_PARSER_H

#include "ofMain.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <memory>

class PlaylistParser {
public:
    enum class PlaylistType {
        UNKNOWN,
        M3U,
        M3U8,
        PLS,
        DIRECT_STREAM
    };
    
    struct StreamInfo {
        std::string url;
        std::string title;
        int bandwidth = 0;  // For HLS variants
        bool isHLS = false;
    };
    
    PlaylistParser();
    ~PlaylistParser();
    
    // Main interface
    std::vector<StreamInfo> parse(const std::string& url);
    PlaylistType detectType(const std::string& url, const std::string& content);
    
private:
    // Type-specific parsers
    std::vector<StreamInfo> parseM3U(const std::string& content);
    std::vector<StreamInfo> parseM3U8(const std::string& content, const std::string& baseUrl);
    std::vector<StreamInfo> parsePLS(const std::string& content);
    
    // Helper functions
    static size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    std::string downloadUrl(const std::string& url);
    std::string getBaseUrl(const std::string& url);
    std::string resolveUrl(const std::string& baseUrl, const std::string& relativeUrl);
    
    // String utility functions
    bool startsWith(const std::string& str, const std::string& prefix) const {
        return str.rfind(prefix, 0) == 0;
    }
    
    bool containsString(const std::string& str, const std::string& substr) const {
        return str.find(substr) != std::string::npos;
    }
    
    std::string toLower(const std::string& str) const {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    std::string trim(const std::string& str) const {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }
    
    // CURL handle
    CURL* curl;
    std::string buffer;
};

#endif // PLAYLIST_PARSER_H
