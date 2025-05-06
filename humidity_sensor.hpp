#pragma once
#include "sensor.hpp"
#include <cmath>
#include <algorithm>

/*
* @brief: The HumiditySensor class is a mock sensor that provides mock data within realistic bounds.
*/
class HumiditySensor : public Sensor {
public:
    HumiditySensor() : Sensor(Type::HUMIDITY) {}

    Data read() const override {
        float humidity = 30.0f;
        humidity += 0.1f * (m_counter % 200 == 0 ? 5.0f : 0.0f); // Create spikes in humidity, Watering?
        humidity = min(100.0f, humidity - 0.01f); // slowly drying soil?
        return {
            humidity,
            Type::HUMIDITY,
            m_counter++
        };
    }
};