#pragma once
// Umbrella header: everything cLog++ provides.
//
//   #include <clogpp/clogpp.hpp>
//   c_log::Logger log;
//   log.info("server.start").kv("port", 8080);

#include "clogpp/core.hpp"
#include "clogpp/daily_file_sink.hpp"
#include "clogpp/file_sink.hpp"
#include "clogpp/formatters.hpp"
#include "clogpp/logger.hpp"
#include "clogpp/rotating_file_sink.hpp"
#include "clogpp/sinks.hpp"
#include "clogpp/spsc_ring_buffer.hpp"
