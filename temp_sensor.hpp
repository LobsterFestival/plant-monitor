#pragma once
#include "sensor.hpp"
#include <cmath>

class TempSensor : public Sensor {
public: 
    TempSensor() : Sensor(Type::TEMPERATURE) {}

    Data read() const override {
        return {
            30.0f + sin(m_counter * 0.01f),
            Type::TEMPERATURE,
            m_counter++
        };
    }
};