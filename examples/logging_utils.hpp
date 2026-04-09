#pragma once

#include <format>
#include <iostream>
#include <string_view>
#include <syncstream>
#include <thread>

static inline auto log() {
    return std::osyncstream(std::cout) << std::this_thread::get_id() << "  ";
}

static inline auto log(std::string_view self) {
    return log() << self << "  ";
}

static inline std::string format_name(std::string_view parent,
                                      std::string_view self) {
    return std::format("   {}.{}", parent, self);
}
