#pragma once
// Resolucion adaptativa: baja la escala de render si los FPS se mantienen bajos y la sube si sobran.
// Ventanas largas (3 s) e histeresis para no cambiar por un tiron aislado. Equivale a AdaptiveResolution.js.
#include <algorithm>
#include <cmath>

namespace Bazuko {

class AdaptiveResolution {
public:
    explicit AdaptiveResolution(float limit = 1.0f) { SetLimit(limit); }

    // Escala maxima elegida por el jugador; la minima es 0,75 (o el limite si es menor).
    void SetLimit(float limit) {
        m_Max = limit;
        m_Min = std::min(limit, 0.75f);
        ratio = limit;
        pressure = 0;
        ResetWindow();
    }

    // `frameMs`: intervalo real entre cuadros. Devuelve true si cambio la escala o la presion.
    bool Sample(float frameMs) {
        if (!std::isfinite(frameMs) || frameMs <= 0.0f) return false;
        m_Elapsed += frameMs;
        ++m_Frames;
        const float fps = static_cast<float>(m_Frames) * 1000.0f / m_Elapsed;
        // Ocho cuadros lentos como minimo: un tiron aislado no cambia la calidad
        const bool severe = fps < 30.0f && m_Elapsed >= 1000.0f && m_Frames >= 8;
        if (m_Elapsed < 3000.0f && !severe) return false;
        m_Elapsed = 0.0f;
        m_Frames = 0;
        const float before = ratio;
        const int previousPressure = pressure;
        if (fps < 45.0f) {
            if (ratio > m_Min) ratio = std::max(m_Min, std::round((ratio - (severe ? 0.3f : 0.15f)) * 100.0f) / 100.0f);
            else pressure = std::min(2, pressure + 1);
            m_FastWindows = 0;
        } else if (fps > 57.0f) {
            if (++m_FastWindows >= 4) {
                if (pressure) --pressure;
                else ratio = std::min(m_Max, std::round((ratio + 0.15f) * 100.0f) / 100.0f);
                m_FastWindows = 0;
            }
        } else {
            m_FastWindows = 0;
        }
        return ratio != before || pressure != previousPressure;
    }

    float ratio = 1.0f;   // escala actual
    int pressure = 0;     // 0..2: ya esta al minimo y aun asi va lento

private:
    void ResetWindow() { m_Elapsed = 0.0f; m_Frames = 0; m_FastWindows = 0; }

    float m_Max = 1.0f;
    float m_Min = 0.75f;
    float m_Elapsed = 0.0f;
    int m_Frames = 0;
    int m_FastWindows = 0;
};

} // namespace Bazuko
