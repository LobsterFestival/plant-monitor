#pragma once
#include "sensor.hpp"
#include <cmath>
#include <algorithm>

class HumiditySensor : public Sensor {
public:
    HumiditySensor() : Sensor(Type::HUMIDITY) {}

    Data read() const override {
        static float humidity = 50.0f;
        humidity += 0.1f * (m_counter % 200 == 0 ? 5.0f : 0.0f); // Create spikes in humidity, Watering?
        humidity = min(100.0f, humidity - 0.01f); // slowly drying soil
        m_counter++;
        return {
            humidity,
            Type::HUMIDITY,
            m_counter++
        };
    }
};