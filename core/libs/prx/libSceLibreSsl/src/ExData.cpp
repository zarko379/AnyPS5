#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <new>
#include <vector>

namespace {
constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int SslClass = 1;
constexpr int SslContextClass = 2;
constexpr int StoreContextClass = 5;
constexpr int ClassCount = 16;

struct ExDataFunctions {
    std::int64_t argl;
    void* argp;
    LibreSslExNew newFunction;
    LibreSslExDup dupFunction;
    LibreSslExFree freeFunction;
};

std::mutex classesLock;
std::array<std::vector<ExDataFunctions>, ClassCount> classes;
std::mutex storeContextLock;
int storeContextIndex = -1;

int NewIndex(int classIndex, const ExDataFunctions& functions) {
    std::lock_guard lock(classesLock);
    auto& registered = classes[classIndex];
    try {
        registered.push_back(functions);
    } catch (const std::bad_alloc&) {
        LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
        return -1;
    }
    return static_cast<int>(registered.size() - 1);
}
}

extern "C" {

int APS5_VABI SSL_get_ex_new_index_nid_postfix(std::int64_t argl, void* argp, LibreSslExNew newFunction, LibreSslExDup dupFunction, LibreSslExFree freeFunction) {
    return NewIndex(SslClass, {argl, argp, newFunction, dupFunction, freeFunction});
}

int APS5_VABI SSL_CTX_get_ex_new_index_nid_postfix(std::int64_t argl, void* argp, LibreSslExNew newFunction, LibreSslExDup dupFunction, LibreSslExFree freeFunction) {
    return NewIndex(SslContextClass, {argl, argp, newFunction, dupFunction, freeFunction});
}

int APS5_VABI SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix(void) {
    static char verifyCallback[] = "SSL for verify callback";
    std::lock_guard lock(storeContextLock);
    if (storeContextIndex < 0) storeContextIndex = NewIndex(StoreContextClass, {0, verifyCallback, nullptr, nullptr, nullptr});
    return storeContextIndex;
}

void APS5_VABI CRYPTO_cleanup_all_ex_data_nid_postfix(void) {
    std::lock_guard lock(classesLock);
    for (auto& registered : classes) std::vector<ExDataFunctions>().swap(registered);
}

void* LibreSslGetExData_nid_no_patch(const LibreSslExData* data, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(data->stack);
    if (!stack || index >= sk_num_nid_postfix(stack)) return nullptr;
    return sk_value_nid_postfix(stack, index);
}

void LibreSslFreeExData_nid_no_patch(int classIndex, void* object, LibreSslExData* data) {
    std::vector<ExDataFunctions> registered;
    {
        std::lock_guard lock(classesLock);
        try {
            registered = classes.at(classIndex);
        } catch (const std::bad_alloc&) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return;
        }
    }
    for (std::size_t index = 0; index < registered.size(); ++index) {
        const ExDataFunctions& functions = registered[index];
        if (!functions.freeFunction) continue;
        const int position = static_cast<int>(index);
        functions.freeFunction(object, LibreSslGetExData_nid_no_patch(data, position), data, position, functions.argl, functions.argp);
    }
    auto* stack = static_cast<LibreSslStack*>(data->stack);
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
    data->stack = nullptr;
}

}
