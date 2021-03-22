//
// Created by Yohsuke Murase on 2020/02/27.
//

#ifndef CARAVAN_SCHEDULER_LOGGER_HPP
#define CARAVAN_SCHEDULER_LOGGER_HPP

#include <iostream>
#include <fstream>
#include <chrono>
#include <mpi.h>

namespace caravan_impl {

template<typename... Args>
#ifndef NDEBUG
void debug_printf(const char *format, Args const &... args) {
  fprintf(stderr, format, args...);
}

#else
void debug_printf(const char *, Args const &...) {
}
#endif

class Logger {
  public:
  Logger(std::chrono::system_clock::time_point _base, int _rank, int _log_level = 2) : base(_base), rank(_rank),
                                                                                       log_level(_log_level) {};

  template<typename... Args>
  void d(const char *format, Args const &... args) {
    if (log_level >= 2) {
      std::string header = "[%.2f @ %d][D] :";
      Out(header, format, args...);
    }
  }

  template<typename... Args>
  void i(const char *format, Args const &... args) {
    if (log_level >= 1) {
      std::string header = "[%.2f @ %d][I] :";
      Out(header, format, args...);
    }
  }

  template<typename... Args>
  void e(const char *format, Args const &... args) {
    if (log_level >= 0) {
      std::string header = "[%.2f @ %d][E] :";
      Out(header, format, args...);
    }
  }

  std::chrono::system_clock::time_point BaseTime() const { return base; }

  int MPIRank() const { return rank; }

  private:
  const std::chrono::system_clock::time_point base;
  int rank;
  int log_level;

  template<typename... Args>
  void Out(const std::string &header, const char *format, Args const &... args) {
    auto end = std::chrono::system_clock::now();
    double d = std::chrono::duration_cast<std::chrono::milliseconds>(end - base).count() / 1000.0;
    debug_printf((header + std::string(format) + std::string("\n")).c_str(), d, rank, args...);
  }
};

}
#endif //CARAVAN_SCHEDULER_LOGGER_HPP
