#pragma once
// Mide el intervalo real entre cuadros (nunca el dt limitado de la simulacion): FPS actual, minimo,
// maximo, promedio y percentiles de los ultimos 600 cuadros. Equivale a FrameMetrics.js del juego web.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Bazuko {

class FrameMetrics {
public:
    struct Recent {
        size_t frames = 0;
        float p95 = 0.0f, p99 = 0.0f;
        float low1Percent = 0.0f;   // FPS medio del 1 % de cuadros mas lentos
        int over100ms = 0;
    };

    FrameMetrics() { Reset(); }

    void Reset() {
        m_RecentIndex = m_RecentCount = 0;
        m_HasPrevious = false;
        frames = 0;
        elapsedMs = 0.0;
        current = min = max = average = frameMs = 0.0f;
    }

    // `nowMs`: reloj monotono en milisegundos. Con `active` falso (pausa) se descarta el intervalo.
    bool Sample(double nowMs, bool active = true) {
        if (!active) {
            m_HasPrevious = false;
            return false;
        }
        const bool had = m_HasPrevious;
        const double previous = m_Previous;
        m_Previous = nowMs;
        m_HasPrevious = true;
        if (!had || nowMs <= previous) return false;
        frameMs = static_cast<float>(nowMs - previous);
        current = 1000.0f / frameMs;
        ++frames;
        elapsedMs += frameMs;
        min = frames == 1 ? current : std::min(min, current);
        max = std::max(max, current);
        average = static_cast<float>(static_cast<double>(frames) * 1000.0 / elapsedMs);
        m_Recent[m_RecentIndex] = frameMs;
        m_RecentIndex = (m_RecentIndex + 1) % m_Recent.size();
        m_RecentCount = std::min(m_Recent.size(), m_RecentCount + 1);
        return true;
    }

    Recent RecentStats() const {
        Recent stats;
        std::vector<float> values(m_Recent.begin(), m_Recent.begin() + static_cast<std::ptrdiff_t>(m_RecentCount));
        std::sort(values.begin(), values.end());
        stats.frames = values.size();
        if (values.empty()) return stats;
        const auto percentile = [&](double p) {
            const size_t index = std::min(values.size() - 1, static_cast<size_t>(std::ceil(static_cast<double>(values.size()) * p)) - 1);
            return values[index];
        };
        stats.p95 = percentile(0.95);
        stats.p99 = percentile(0.99);
        const size_t slowestCount = std::max<size_t>(1, static_cast<size_t>(std::ceil(static_cast<double>(values.size()) * 0.01)));
        float sum = 0.0f;
        for (size_t index = values.size() - slowestCount; index < values.size(); ++index) sum += values[index];
        const float lowMean = sum / static_cast<float>(slowestCount);
        stats.low1Percent = lowMean > 0.0f ? 1000.0f / lowMean : 0.0f;
        stats.over100ms = static_cast<int>(std::count_if(values.begin(), values.end(), [](float value) { return value > 100.0f; }));
        return stats;
    }

    size_t frames = 0;
    double elapsedMs = 0.0;
    float current = 0.0f, min = 0.0f, max = 0.0f, average = 0.0f, frameMs = 0.0f;

private:
    std::array<float, 600> m_Recent{};
    size_t m_RecentIndex = 0;
    size_t m_RecentCount = 0;
    double m_Previous = 0.0;
    bool m_HasPrevious = false;
};

} // namespace Bazuko
