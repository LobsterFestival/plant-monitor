#pragma once
#include <array>
#include <cstdint>

template<typename T, size_t N>
class MovingAverage {
public:
    MovingAverage() {
        reset();
        if constexpr (std::is_arithmetic_v<T>) {
            m_samples.fill(0);
        }
    }

    T addSample(T sample) {
        m_sum -= m_samples[m_idx];
        m_sum += sample;
        m_samples[m_idx] = sample;
        m_idx = (m_idx + 1) % N; // Circular buffer
        return m_sum / static_cast<T>(N);
    }
    
    T getAverage() const {
        return m_sum / static_cast<T>(N);
    }

    void reset() {
        m_samples.fill(0);
        m_sum = 0;
        m_idx = 0;
    }

private:
    std::array<T, N> m_samples; // using std::array because its stack allocated, no dynamic memory action inside of tasks.
    size_t m_idx = 0;
    T m_sum;
};