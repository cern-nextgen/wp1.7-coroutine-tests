#ifndef COROUTINETESTS_EXAMPLES_LOGGING_UTILS_HPP
#define COROUTINETESTS_EXAMPLES_LOGGING_UTILS_HPP

#include <format>
#include <iostream>
#include <string_view>
#include <thread>

static inline std::ostream& log() {
    return std::cout << std::this_thread::get_id() << "  ";
}

static inline std::ostream& log(std::string_view self) {
    return std::cout << std::this_thread::get_id() << "  " << self << "  ";
}

static inline std::string format_name(std::string_view parent,
                                      std::string_view self) {
    return std::format("   {}.{}", parent, self);
}

#endif  // COROUTINETESTS_EXAMPLES_LOGGING_UTILS_HPP
