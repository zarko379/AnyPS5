#include <unicode/udata.h>
#include <unicode/ures.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU DATA FILES: %s\n", message);
        std::abort();
    }
}

void SetDataEnvironment(const std::filesystem::path& directory) {
#ifdef _WIN32
    _putenv_s("ICU_DATA", directory.string().c_str());
#else
    setenv("ICU_DATA", directory.string().c_str(), 1);
#endif
}

bool Mapped(const std::filesystem::path& file) {
#ifdef _WIN32
    const std::wstring suffix = (file.parent_path().filename() / file.filename()).wstring();
    MEMORY_BASIC_INFORMATION info;
    const char* address = nullptr;
    wchar_t name[MAX_PATH * 2];
    while (VirtualQuery(address, &info, sizeof(info)) == sizeof(info)) {
        if (info.Type == MEM_MAPPED) {
            const DWORD length = K32GetMappedFileNameW(GetCurrentProcess(), info.BaseAddress, name, MAX_PATH * 2);
            const std::wstring mapped(name, length);
            if (length >= suffix.size() && mapped.compare(length - suffix.size(), suffix.size(), suffix) == 0) return true;
        }
        address = static_cast<const char*>(info.BaseAddress) + info.RegionSize;
    }
    return false;
#else
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
        if (line.find(file.string()) != std::string::npos) return true;
    return false;
#endif
}

}

int main() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / ("anyps5_icu_data_files_" + std::to_string(std::rand()) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(&directory)));
    std::filesystem::create_directories(directory);
    const std::filesystem::path stray = directory / "icudt68l.dat";
    std::filesystem::copy_file(ICU_DATA_FILE, stray, std::filesystem::copy_options::overwrite_existing);
    SetDataEnvironment(directory);
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(directory);

    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    Require(U_SUCCESS(status), "file access is set");
    UResourceBundle* bundle = ures_openDirect(nullptr, "zz_ZZ", &status);
    Require(bundle == nullptr && status == U_MISSING_RESOURCE_ERROR, "a missing bundle is reported missing");
    Require(!Mapped(stray), "a missing item never maps an icudt68l.dat from the data directory or the working directory");
    status = U_ZERO_ERROR;
    bundle = ures_openDirect(nullptr, "root", &status);
    Require(bundle != nullptr && U_SUCCESS(status), "the embedded data still serves root");
    ures_close(bundle);
    Require(!Mapped(stray), "the embedded data never maps the stray copy");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::puts("guest_icu_data_files: ok");
    return 0;
}
