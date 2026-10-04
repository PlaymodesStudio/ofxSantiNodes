#ifndef currentTime_h
#define currentTime_h

#include "ofxOceanodeNodeModel.h"

#include <ctime>

class currentTime : public ofxOceanodeNodeModel {
public:
    currentTime() : ofxOceanodeNodeModel("Current Time") {}

    void setup() override {
        description = "Outputs the current local date and time. Weekday uses Monday = 1 through Sunday = 7.";

        const auto flags = ofxOceanodeParameterFlags_DisableSavePreset;
        addOutputParameter(year.set("Year", 0, 0, 9999), flags);
        addOutputParameter(month.set("Month", 0, 1, 12), flags);
        addOutputParameter(day.set("Day", 0, 1, 31), flags);
        addOutputParameter(weekday.set("Weekday", 0, 1, 7), flags);
        addOutputParameter(hour.set("Hour", 0, 0, 23), flags);
        addOutputParameter(minute.set("Minute", 0, 0, 59), flags);
        addOutputParameter(second.set("Second", 0, 0, 59), flags);

        updateTime();
    }

    void update(ofEventArgs &args) override {
        updateTime();
    }

private:
    void updateTime() {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};

#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        year = localTime.tm_year + 1900;
        month = localTime.tm_mon + 1;
        day = localTime.tm_mday;
        weekday = ((localTime.tm_wday + 6) % 7) + 1;
        hour = localTime.tm_hour;
        minute = localTime.tm_min;
        second = localTime.tm_sec;
    }

    ofParameter<int> year;
    ofParameter<int> month;
    ofParameter<int> day;
    ofParameter<int> weekday;
    ofParameter<int> hour;
    ofParameter<int> minute;
    ofParameter<int> second;
};

#endif /* currentTime_h */
