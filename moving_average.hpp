#pragma once
#include <array>
#include <cstdint>


template<size_t N>
class MovingAverage {
public:
    MovingAverage() = default;

    float process(float sample) {
        sum -= buffer[index];
        sum += sample;
        buffer[index] = sample;
        index = (index + 1) % N;
        return sum / N;
    }

    float current() const { return sum / N;  }

private:
    std::array<float, N> buffer{}; // zero init, using std::array because its stack allocated, no dynamic memory action inside of tasks.
    size_t index = 0;
    float sum = 0.0f;
};