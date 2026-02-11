#pragma once
#include <string>
#include <iostream>
namespace c_log {
// Debug helper (quick log to stderr, compile-out in release if defined)
#ifndef CLOG_DISABLE_DEBUG
inline void debug_log(const std::string& msg) { std::cerr << "[cLog++-debug] " << msg << std::endl; }
#else
inline void debug_log(const std::string&) {}
#endif
}
