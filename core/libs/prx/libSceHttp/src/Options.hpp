#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP

#include "SceTypes.hpp"
#include "prx/libSceHttp/include/Http.hpp"
#include <optional>

struct HttpOptions {
    bool autoRedirect = true;
    bool authEnabled = false;
    bool cookieEnabled = false;
    bool nonblock = false;
    bool acceptEncodingGzip = false;
    bool delayBuildRequest = false;
    HttpEpollHandle epoll = nullptr;
    void* epollArgument = nullptr;
    HttpCookieSendCallback cookieSendCallback = nullptr;
    void* cookieSendArgument = nullptr;
};

void AddHttpContext(int contextId);
void RemoveHttpContext(int contextId);
bool HttpInitialised();
void AddHttpTemplate(int templateId, int contextId);
void AddHttpChild(int id, int parentId);
void RemoveHttpObject(int id);
std::optional<HttpOptions> FindHttpOptions(int id);
bool SetHttpOption(int id, bool HttpOptions::*option, bool enable);
bool SetHttpContextAcceptEncodingGzip(int contextId, bool enable);
bool SetHttpEpollBinding(int id, HttpEpollHandle epoll, void* argument);
bool SetHttpCookieSendCallback(int id, HttpCookieSendCallback callback, void* argument);

#endif
