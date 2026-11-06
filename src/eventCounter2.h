#pragma once

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

class eventCounter2 : public ofxOceanodeNodeModel {
public:
    eventCounter2() : ofxOceanodeNodeModel("Event Counter 2") {
        description =
            "Counts input events, measures their interval in milliseconds and shows their values over time.\n\n"
            "**Inputs and controls**\n\n"
            "**[ Input]** Each notification is one event, including repeated values and empty vectors. "
            "All elements share one counter and timer.\n\n"
            "**[ Vec]** Repeats Count to match the input length; off outputs one integer.\n\n"
            "**[ Reset]** Clears count and timing immediately and cancels Reset Next. No Tick is emitted.\n\n"
            "**[ Reset Next]** The next event starts at count zero and IntervalMs zero, and emits Tick.\n\n"
            "**[ Verbose]** Logs count, interval in ms, FPS and the complete input vector.\n\n"
            "**[ Show]** Opens the Datagram window. Sample freezes or resumes graph recording; "
            "counting, timing and logging continue. Sampling starts enabled and continues while hidden.\n\n"
            "**Outputs**\n\n"
            "**[ Count]** Event count; the first event after creation or Reset is one.\n\n"
            "**[ IntervalMs]** Milliseconds between the latest two events, held until the next event.\n\n"
            "**[ FPS]** Event rate, 1000 / IntervalMs. First events and zero intervals output zero.\n\n"
            "**[ Tick]** One trigger per event, after all numerical outputs are updated.\n\n"
            "**Datagram**\n\n"
            "Shows the latest 1000 sampled events at their actual times in ms, with one colored trace per "
            "vector element and automatic value scaling. Dense traces are reduced for display while preserving "
            "peaks; dots appear for sparse data. All sampled events remain in history. Pauses in sampling leave time gaps. Reset preserves "
            "the graph. Preset recall starts a fresh count, timing reference and graph without emitting Tick. "
            "Uses real elapsed time independently of transport playback.";

        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(vec.set("Vec", false));
        addParameter(reset.set("Reset"));
        addParameter(resetNext.set("Reset Next"));
        addParameter(verbose.set("Verbose", false));
        addParameter(show.set("Show", false));
        addOutputParameter(count.set("Count", {0}, {0}, {INT_MAX}));
        addOutputParameter(intervalMs.set("IntervalMs", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(fps.set("FPS", 0.0f, 0.0f, FLT_MAX));
        addOutputParameter(tick.set("Tick"));

        listeners.push(input.newListener([this](std::vector<float> &values) {
            if(!loadingPreset && !recallingParameters) recordEvent(values, Clock::now());
        }));
        listeners.push(vec.newListener([this](bool &) { publishCount(); }));
        listeners.push(reset.newListener([this]() { resetMeasurement(); }));
        listeners.push(resetNext.newListener([this]() { resetOnNextEvent = true; }));
    }

    void draw(ofEventArgs &) override {
        if(!show.get()) return;
        const std::string title = (canvasID == "Canvas" ? "" : canvasID + "/") +
            "Event Counter 2 " + ofToString(getNumIdentifier()) + " - Datagram";
        ImGui::SetNextWindowSize(ImVec2(680, 360), ImGuiCond_FirstUseEver);
        bool open = show.get();
        if(ImGui::Begin(title.c_str(), &open)) {
            ImGui::Checkbox("Sample", &sampling);
            ImGui::SameLine();
            ImGui::TextDisabled("%s | %zu / %zu events", sampling ? "Recording" : "Frozen",
                                history.size(), HistoryLimit);
            ImGui::Text("Count: %d   Interval: %.3f ms   FPS: %.2f",
                        eventCount, intervalMs.get(), fps.get());
            drawDatagram();
        }
        ImGui::End();
        if(!open) show = false;
    }

    void presetWillBeLoaded() override { loadingPreset = true; }
    void loadBeforeConnections(ofJson &) override { loadingPreset = true; }
    void presetRecallBeforeSettingParameters(ofJson &) override { recallingParameters = true; }
    void presetRecallAfterSettingParameters(ofJson &) override {
        clearRuntime();
        recallingParameters = false;
    }
    void presetHasLoaded() override {
        clearRuntime();
        loadingPreset = false;
        recallingParameters = false;
    }

private:
    using Clock = std::chrono::steady_clock;
    struct Sample {
        double timeMs;
        std::vector<float> values;
    };
    static constexpr std::size_t HistoryLimit = 1000;
    static constexpr std::size_t PlotPointBudget = 8192;
    struct PlotStrip {
        ImU32 color;
        std::vector<ImVec2> points; // Normalized to the plot; independent of window position.
    };

    ofParameter<std::vector<float>> input;
    ofParameter<bool> vec, verbose, show;
    ofParameter<void> reset, resetNext, tick;
    ofParameter<std::vector<int>> count;
    ofParameter<float> intervalMs, fps;
    int eventCount = 0;
    bool resetOnNextEvent = false;
    bool hasPreviousEvent = false;
    bool sampling = true;
    bool loadingPreset = false;
    bool recallingParameters = false;
    Clock::time_point previousEvent{};
    Clock::time_point graphOrigin = Clock::now();
    std::deque<Sample> history;
    bool plotDirty = true;
    int plotBuckets = 0;
    double plotMinimum = 0.0, plotMaximum = 1.0, plotSpanMs = 1.0;
    bool plotHasValues = false;
    bool plotMarkers = false;
    std::vector<PlotStrip> plotStrips;
    std::vector<ImVec2> drawPoints;
    // Listeners must be destroyed before the parameters/state they access.
    ofEventListeners listeners;

    void publishCount() {
        count = std::vector<int>(vec.get() ? input.get().size() : 1, eventCount);
    }

    void resetMeasurement() {
        eventCount = 0;
        resetOnNextEvent = false;
        hasPreviousEvent = false;
        intervalMs = 0.0f;
        fps = 0.0f;
        publishCount();
    }

    void clearRuntime() {
        resetMeasurement();
        history.clear();
        plotDirty = true;
        graphOrigin = Clock::now();
        sampling = true;
    }

    void recordEvent(const std::vector<float> &values, Clock::time_point now) {
        double elapsedMs = 0.0;
        if(resetOnNextEvent) {
            eventCount = 0;
            resetOnNextEvent = false;
        } else {
            if(eventCount < INT_MAX) ++eventCount;
            if(hasPreviousEvent)
                elapsedMs = std::max(0.0, std::chrono::duration<double, std::milli>(now - previousEvent).count());
        }
        previousEvent = now;
        hasPreviousEvent = true;

        if(sampling) {
            history.push_back({std::chrono::duration<double, std::milli>(now - graphOrigin).count(), values});
            if(history.size() > HistoryLimit) history.pop_front();
            plotDirty = true;
        }
        intervalMs = static_cast<float>(elapsedMs);
        fps = elapsedMs > 0.0 ? static_cast<float>(1000.0 / elapsedMs) : 0.0f;
        publishCount();

        if(verbose.get()) {
            std::ostringstream message;
            message << "Count: " << eventCount << " | Interval: " << elapsedMs
                    << " ms | FPS: " << fps.get() << " | Input: [";
            for(std::size_t i = 0; i < values.size(); ++i) {
                if(i) message << ", ";
                message << values[i];
            }
            message << "]";
            ofLogNotice("Event Counter 2") << message.str();
        }
        tick.trigger();
    }

    static ImU32 traceColor(std::size_t channel) {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(std::fmod(0.55f + channel * 0.618034f, 1.0f), 0.65f, 0.95f, r, g, b);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
    }

    void rebuildPlot(int horizontalPixels) {
        // Bound visible geometry across all channels, not per channel. Retain
        // first/min/max/last in each time bucket so short spikes survive reduction.
        if(!plotDirty && plotBuckets == horizontalPixels) return;
        plotDirty = false;
        plotBuckets = horizontalPixels;
        plotStrips.clear();
        plotMinimum = std::numeric_limits<double>::infinity();
        plotMaximum = -std::numeric_limits<double>::infinity();
        std::size_t channels = 0, valueCount = 0;
        for(const auto &sample : history) {
            channels = std::max(channels, sample.values.size());
            valueCount += sample.values.size();
            for(float value : sample.values) if(std::isfinite(value)) {
                plotMinimum = std::min(plotMinimum, static_cast<double>(value));
                plotMaximum = std::max(plotMaximum, static_cast<double>(value));
            }
        }
        plotHasValues = std::isfinite(plotMinimum);
        if(!plotHasValues) { plotMinimum = 0.0; plotMaximum = 1.0; }
        const double padding = plotMaximum > plotMinimum ? (plotMaximum - plotMinimum) * 0.05 : std::max(0.5, std::abs(plotMinimum) * 0.05);
        plotMinimum -= padding;
        plotMaximum += padding;
        const double firstMs = history.empty() ? 0.0 : history.front().timeMs;
        plotSpanMs = history.empty() ? 1.0 : std::max(1.0, history.back().timeMs - firstMs);
        plotMarkers = valueCount <= 256;
        const int buckets = static_cast<int>(std::max<std::size_t>(1,
            std::min<std::size_t>(horizontalPixels, PlotPointBudget / std::max<std::size_t>(1, channels) / 4)));
        struct Candidate { std::size_t index; ImVec2 point; };
        for(std::size_t channel = 0; channel < channels; ++channel) {
            const ImU32 color = traceColor(channel);
            PlotStrip strip{color, {}};
            int currentBucket = -1;
            std::array<Candidate, 4> selected{}; // first, min, max, last
            auto flushBucket = [&]() {
                if(currentBucket < 0) return;
                auto ordered = selected;
                std::sort(ordered.begin(), ordered.end(), [](const Candidate &a, const Candidate &b) { return a.index < b.index; });
                std::size_t previousIndex = std::numeric_limits<std::size_t>::max();
                for(const auto &candidate : ordered) if(candidate.index != previousIndex) {
                    strip.points.push_back(candidate.point);
                    previousIndex = candidate.index;
                }
                currentBucket = -1;
            };
            auto flushStrip = [&]() {
                flushBucket();
                if(!strip.points.empty()) {
                    plotStrips.push_back(std::move(strip));
                    strip = PlotStrip{color, {}};
                }
            };
            std::size_t index = 0;
            for(const auto &sample : history) {
                const std::size_t eventIndex = index++;
                if(channel >= sample.values.size() || !std::isfinite(sample.values[channel])) {
                    flushStrip(); // Never join across a missing or invalid channel value.
                    continue;
                }
                const double x = std::clamp((sample.timeMs - firstMs) / plotSpanMs, 0.0, 1.0);
                const int bucket = std::min(buckets - 1, static_cast<int>(x * buckets));
                Candidate candidate{eventIndex, ImVec2(static_cast<float>(x),
                    static_cast<float>((static_cast<double>(sample.values[channel]) - plotMinimum) / (plotMaximum - plotMinimum)))};
                if(bucket != currentBucket) {
                    flushBucket();
                    currentBucket = bucket;
                    selected.fill(candidate);
                } else {
                    if(candidate.point.y < selected[1].point.y) selected[1] = candidate;
                    if(candidate.point.y > selected[2].point.y) selected[2] = candidate;
                    selected[3] = candidate;
                }
            }
            flushStrip();
        }
    }

    void drawDatagram() {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 size(std::max(180.0f, available.x), std::max(130.0f, available.y));
        ImGui::InvisibleButton("Datagram", size);
        if(!ImGui::IsItemVisible()) return;
        auto *draw = ImGui::GetWindowDrawList();
        const ImVec2 end(origin.x + size.x, origin.y + size.y);
        draw->AddRectFilled(origin, end, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
        draw->PushClipRect(origin, end, true);

        const ImVec2 topLeft(origin.x + 70.0f, origin.y + 22.0f);
        const ImVec2 bottomRight(end.x - 12.0f, end.y - 30.0f);
        const float width = bottomRight.x - topLeft.x;
        const float height = bottomRight.y - topLeft.y;
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Border);
        draw->AddText(ImVec2(origin.x + 8, origin.y + 4), textColor, "Value");

        rebuildPlot(std::max(1, static_cast<int>(width)));

        for(int i = 0; i <= 4; ++i) {
            const float fraction = i / 4.0f;
            const float x = topLeft.x + width * fraction;
            const float y = bottomRight.y - height * fraction;
            draw->AddLine(ImVec2(x, topLeft.y), ImVec2(x, bottomRight.y), gridColor);
            draw->AddLine(ImVec2(topLeft.x, y), ImVec2(bottomRight.x, y), gridColor);
            char label[48];
            std::snprintf(label, sizeof(label), "%.3g", plotMinimum + (plotMaximum - plotMinimum) * fraction);
            draw->AddText(ImVec2(origin.x + 4, y - ImGui::GetTextLineHeight() * 0.5f), textColor, label);
            std::snprintf(label, sizeof(label), "%.3g", plotSpanMs * fraction);
            const float labelWidth = ImGui::CalcTextSize(label).x;
            draw->AddText(ImVec2(x - labelWidth * fraction, bottomRight.y + 4), textColor, label);
        }
        const char *timeLabel = "Time (ms from oldest sample)";
        draw->AddText(ImVec2(bottomRight.x - ImGui::CalcTextSize(timeLabel).x, end.y - ImGui::GetTextLineHeight()), textColor, timeLabel);

        if(!plotHasValues) {
            draw->AddText(ImVec2(topLeft.x + 10, topLeft.y + 10), textColor,
                          history.empty() ? "Waiting for sampled events" : "No finite values to plot");
        }
        draw->PushClipRect(topLeft, bottomRight, true);
        for(const auto &strip : plotStrips) {
            drawPoints.resize(strip.points.size());
            for(std::size_t i = 0; i < strip.points.size(); ++i)
                drawPoints[i] = ImVec2(topLeft.x + strip.points[i].x * width, bottomRight.y - strip.points[i].y * height);
            if(drawPoints.size() >= 2)
                draw->AddPolyline(drawPoints.data(), static_cast<int>(drawPoints.size()), strip.color, 0, 1.0f);
            if(plotMarkers || drawPoints.size() == 1)
                for(const auto &point : drawPoints) draw->AddCircleFilled(point, 2.5f, strip.color, 8);
        }
        draw->PopClipRect();
        draw->PopClipRect();
    }
};
