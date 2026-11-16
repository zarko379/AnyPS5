#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

#ifdef _WIN32
FILE* OpenChild(const std::string& self) { return _popen((std::string("\"\"") + self + "\" abort 2>&1\"").c_str(), "r"); }
int CloseChild(FILE* child) { return _pclose(child); }
#else
FILE* OpenChild(const std::string& self) { return popen((std::string("\"") + self + "\" abort 2>&1").c_str(), "r"); }
int CloseChild(FILE* child) { return pclose(child); }
#endif

void Require(bool condition, const char* message, const std::string& output) {
    if (condition) return;
    std::fprintf(stderr, "libc internal _Rng_abort: %s\noutput:\n%s\n", message, output.c_str());
    std::abort();
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "abort") == 0) {
        libSceLibcInternalInventory_237_nid_no_patch("random engine precondition violated");
        std::puts("_Rng_abort returned");
        std::fflush(stdout);
        return 0;
    }
    FILE* child = OpenChild(argv[0]);
    Require(child != nullptr, "the child process starts", "");
    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), child) != nullptr) output += buffer;
    const int status = CloseChild(child);
    Require(output.find("random engine precondition violated\n") != std::string::npos, "the message and a newline reach stderr", output);
    Require(output.find("_Rng_abort returned") == std::string::npos, "the call does not return", output);
    Require(status != 0, "the child terminates abnormally", output);
    std::puts("libc internal _Rng_abort checks passed");
    return 0;
}
