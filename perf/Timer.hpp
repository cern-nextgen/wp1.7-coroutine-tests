#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <sys/time.h>
#include <sys/types.h>

/**
 * A small timing class.
 * It basically remembers a reference time (by default the time of
 * its construction) and gives the elapsed time since then.
 * The reset method allows to reset the reference time to the current time
 */
class Timer {
public:
  enum reset_t { keepRunning, resetCounter };

  /**
   * Constructor
   */
  Timer();

  /**
   * Destructor
   */
  virtual ~Timer() = default;

  /**
   * Gives elapsed time in microseconds with respect to the reference time
   * optionally resets the counter.
   */
  int64_t usecs(reset_t reset = keepRunning);

  /**
   * Gives elapsed time in seconds (with microsecond precision)
   * with respect to the reference time. Optionally resets the counter.
   */
  double secs(reset_t reset = keepRunning);

  /**
   * Resets the Timer reference's time to the current time.
   */
  void reset();

private:
  /**
   * Reference time for this timer
   */
  timeval m_reference;
}; // class Timer

#endif // ndef PERF_TIMER_H