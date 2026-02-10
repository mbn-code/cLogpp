#include "../include/logger.hpp"
#include "../include/file_sink.hpp"
#include <fstream>
#include <cassert>

int main() {
    const char* fname = "test.log";
    remove(fname); // Clean up before test.
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.add_sink(std::make_unique<c_log::FileSink>(fname));
        log.info("write_to_file").kv("val", 42);
    } // Logger destroyed, should flush
    std::ifstream in(fname);
    std::string line;
    std::getline(in, line);
    assert(line.find("write_to_file") != std::string::npos);
    assert(line.find("42") != std::string::npos);
    return 0;
}
