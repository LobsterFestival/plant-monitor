#pragma once
#include <cstdint>
#include <string>

/*
* @brief: The Sensor class is the base class for all mock sensors, it provides the structure for sensor Data.
* Currently has an enum relating to sensor type for convinence in debugging, but is an example of coupled code.
* IMPROVEMENT: sensor class should contain the moving average filter that operates on, imagine if there are 2 temp sensors, logic becomes more complex.
*/
class Sensor {
public:
    enum class Type { TEMPERATURE, LIGHT, HUMIDITY };

    struct Data {
        float value;
        Type type;
        uint64_t timestamp;
    };

    Type getType() const {
        return m_type;
    }

    Sensor(Type type) : m_type(type) {}
    virtual ~Sensor() = default;
    virtual Data read() const = 0;

protected:
    Type m_type;
    mutable uint32_t m_counter = 0;
};