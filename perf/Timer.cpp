

// Taken from https://gitlab.cern.ch/cta/CTA/-/blob/main/common/Timer.hpp
#include "Timer.hpp"

//------------------------------------------------------------------------------
// constructor
//------------------------------------------------------------------------------
Timer::Timer() {
    reset();
}

//------------------------------------------------------------------------------
// usecs
//------------------------------------------------------------------------------
int64_t Timer::usecs(reset_t reset) {
    timeval now;
    gettimeofday(&now, nullptr);
    int64_t ret = ((now.tv_sec * 1000000) + now.tv_usec) -
                  ((m_reference.tv_sec * 1000000) + m_reference.tv_usec);
    if (reset == resetCounter) {
        m_reference = now;
    }
    return ret;
}

//------------------------------------------------------------------------------
// secs
//------------------------------------------------------------------------------
double Timer::secs(reset_t reset) {
    return usecs(reset) * 0.000001;
}

//------------------------------------------------------------------------------
// reset
//------------------------------------------------------------------------------
void Timer::reset() {
    gettimeofday(&m_reference, nullptr);
}
