#pragma once
#include "sensor.hpp"
#include <cmath>

/*
* @brief: The TempSensor class is a mock sensor that provides mock data within realistic bounds.
*/
class TempSensor : public Sensor {
public: 
    TempSensor() : Sensor(Type::TEMPERATURE) {}

    Data read() const override {
        float base = 25.0f + 5.0f * sin(m_counter * 0.001f); // Baseline periodic fluctuations
        float noise = 0.5f * (rand() % 100 - 50) / 50.0f; // random fluctuations
        float temp = base + noise;
        return {
            temp,
            Type::TEMPERATURE,
            m_counter++
        };
    }
};