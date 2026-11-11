#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int SkipReturnCode = 77;
constexpr std::uint32_t Width = 320;
constexpr std::uint32_t Height = 180;
constexpr std::uint32_t Pitch = 384;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::size_t BlockBytes = 1024u * 1024u;
constexpr std::size_t BufferOffset = 32768u;

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    Require(_putenv_s(name, value.c_str()) == 0, std::string("cannot set ") + name);
#else
    Require(setenv(name, value.c_str(), 1) == 0, std::string("cannot set ") + name);
#endif
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

AgcDriver::DisplayBuffer Prepare(std::uint8_t* memory) {
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<std::uint64_t>(memory), Bgra8, Width, Height, AgcDriver::DisplayTilingGcn, Pitch};
    buffer.alignment = 4096;
    const auto bytes = AgcDriver::DisplayBufferSize(buffer);
    Require(bytes + BufferOffset <= BlockBytes, "the display buffer does not fit the guest block");
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const std::uint32_t word = (x & 255u) | ((y & 255u) << 8u) | (255u << 24u);
            std::memcpy(memory + AgcDriver::DisplayBufferOffset(buffer, x, y), &word, sizeof(word));
        }
    }
    return buffer;
}

std::size_t CountFiles(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        static_cast<void>(entry);
        ++count;
    }
    return count;
}

bool Minimized(SDL_Window* window) {
    SDL_PumpEvents();
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
}

#ifdef _WIN32
HWND Handle(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    Require(SDL_GetWindowWMInfo(window, &info) == SDL_TRUE, SDL_GetError());
    return info.info.win.window;
}
#endif

void Minimize(SDL_Window* window) {
#ifdef _WIN32
    ShowWindow(Handle(window), SW_SHOWMINNOACTIVE);
#else
    SDL_ShowWindow(window);
    SDL_MinimizeWindow(window);
#endif
}

void Restore(SDL_Window* window) {
#ifdef _WIN32
    ShowWindow(Handle(window), SW_SHOWNOACTIVATE);
#else
    SDL_RestoreWindow(window);
#endif
    for (int attempt = 0; attempt < 400 && Minimized(window); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(!Minimized(window), "the window did not leave the minimized state");
}

int Run(bool zeroDrawable) {
    VideoOutTestEnvironment environment;
    const auto screenshots = std::filesystem::temp_directory_path() / (zeroDrawable ? "anyps5_minimized_present_drawable" : "anyps5_minimized_present_surface");
    std::filesystem::remove_all(screenshots);
    std::filesystem::create_directories(screenshots);
    SetEnvironment("ANYPS5_SCREENSHOTS", screenshots.string());

    SDL_SetMainReady();
    const auto noDisplay = [](const char* error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw std::runtime_error(error);
        std::printf("skipped, no display: %s\n", error);
        return SkipReturnCode;
    };
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return noDisplay(SDL_GetError());
    AgcDriverLockVulkanLoader_nid_postfix();
    auto* window = SDL_CreateWindow("minimized present", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    AgcDriverUnlockVulkanLoader_nid_postfix();
    if (window == nullptr) return noDisplay(SDL_GetError());
    Minimize(window);
    for (int attempt = 0; attempt < 200 && !Minimized(window); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!Minimized(window)) return noDisplay("the window manager did not minimize the window");
    unsigned count = 0;
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr) == SDL_TRUE, SDL_GetError());
    std::vector<const char*> extensions(count);
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data()) == SDL_TRUE, SDL_GetError());
    const auto surface = [](void* context, VkInstance instance) {
        VkSurfaceKHR created = VK_NULL_HANDLE;
        Require(SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(context), instance, &created) == SDL_TRUE, SDL_GetError());
        return created;
    };
    const auto windowSize = [](void* context, std::uint32_t* width, std::uint32_t* height) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(context), &drawableWidth, &drawableHeight);
        *width = drawableWidth > 0 ? static_cast<std::uint32_t>(drawableWidth) : 0;
        *height = drawableHeight > 0 ? static_cast<std::uint32_t>(drawableHeight) : 0;
    };
    const auto minimizedZero = [](void* context, std::uint32_t* width, std::uint32_t* height) {
        if ((SDL_GetWindowFlags(static_cast<SDL_Window*>(context)) & SDL_WINDOW_MINIMIZED) != 0) {
            *width = 0;
            *height = 0;
            return;
        }
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(context), &drawableWidth, &drawableHeight);
        *width = drawableWidth > 0 ? static_cast<std::uint32_t>(drawableWidth) : 0;
        *height = drawableHeight > 0 ? static_cast<std::uint32_t>(drawableHeight) : 0;
    };
    const AgcDriver::PresentationWindow target{window, extensions, surface, zeroDrawable ? +minimizedZero : +windowSize, Width, Height};
    try {
        AgcDriverAttachWindow_nid_postfix(target);
    } catch (const AgcDriver::VulkanUnavailable& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return SkipReturnCode;
    }

    GuestBlock block;
    const auto buffer = Prepare(block.Data() + BufferOffset);
    const auto present = [&] {
        bool ready = false;
        AgcDriverPresentBuffer_nid_postfix(target, buffer, nullptr, [](void* context) { *static_cast<bool*>(context) = true; }, &ready);
        Require(ready, "the flip did not complete");
    };
    AgcDriver::Screenshot::Request();
    for (int frame = 0; frame < 8; ++frame) present();
    Require(Minimized(window), "the window left the minimized state on its own");
    Require(CountFiles(screenshots) == 0u, "a minimized window presented a frame");

    Restore(window);
    for (int attempt = 0; attempt < 400 && CountFiles(screenshots) == 0u; ++attempt) {
        present();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Require(CountFiles(screenshots) == 1u, "the restored window did not present a frame");

    AgcDriverReleaseWindow_nid_postfix(window);
    LibcRunShutdown_nid_postfix();
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::filesystem::remove_all(screenshots);
    std::puts(zeroDrawable ? "minimized present passed with a zero drawable size" : "minimized present passed with a zero surface extent");
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const bool zeroDrawable = argc > 1 && std::strcmp(argv[1], "drawable") == 0;
        Require(argc == 1 || zeroDrawable || std::strcmp(argv[1], "surface") == 0, "the mode is surface or drawable");
        return Run(zeroDrawable);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
