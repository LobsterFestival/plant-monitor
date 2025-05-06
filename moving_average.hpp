#pragma once
#include <array>
#include <cstdint>
#include <concepts>

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T, size_t N>
    requires Arithmetic<T>

class MovingAverage {
public:
    MovingAverage() : m_samples{ 0 }, m_sum(0), m_idx(0) {}

    /*
    * @brief 
    * @return
    */
    T addSample(const T sample) {
        m_sum -= m_samples[m_idx];
        m_sum += sample;
        m_samples[m_idx] = sample;
        m_idx = (m_idx + 1) % N; // Circular buffer
        m_average = m_sum / static_cast<T>(N);
        return m_average;
    }
    
    /*
    * @brief
    * @return
    */
    T getAverage() const {
        return m_average;
    }

    /*
    * @brief
    * @return
    */
    void reset() {
        m_samples.fill(0);
        m_sum = 0;
        m_idx = 0;
    }

private:
    std::array<T, N> m_samples; // using std::array because its stack allocated, no dynamic memory action inside of tasks.
    size_t m_idx = 0;
    T m_sum;
    T m_average; // used to prevent recalculations
};