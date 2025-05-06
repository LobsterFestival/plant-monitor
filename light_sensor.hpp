#pragma once
#include "sensor.hpp"
#include <cmath>
#include <algorithm>

class LightSensor : public Sensor {
public:
    LightSensor() : Sensor(Type::LIGHT) {}
    
    Data read() const override {
        // more intresting light level fluctuations.
        float baseline = 500.0f + 300.0f * sin(m_counter * 0.005); // sin for periodic fluctuations
        float noise = 150.0f * sin(m_counter * 0.1f) * (rand() % 100 / 100.0f); // little bit of RNG for spice
        return {
            max(0.0f, baseline + noise), // ensure light is never negative
            Type::LIGHT,
            m_counter++
        };
    }
};