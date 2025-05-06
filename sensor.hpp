#pragma once
#include <cstdint>
#include <string>

class Sensor {
public:
    enum class Type { TEMPERATURE, LIGHT, HUMIDITY };

    struct Data {
        float value;
        Type type;
        uint32_t timestamp;
    };
    Sensor(Type type) : m_type(type) {}
    virtual ~Sensor() = default;
    virtual Data read() const = 0;

protected:
    Type m_type;
    mutable uint32_t m_counter = 0;
};