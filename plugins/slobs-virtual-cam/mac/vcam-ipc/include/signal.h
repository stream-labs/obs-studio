#ifndef SIGNAL_H
#define SIGNAL_H

#include <cstdint>
#include <string>
#include <vector>

#define SIGNAL(CallbackName, ...) \
    public: \
        using CallbackName##CallbackT = void (*)(void *userData, __VA_ARGS__); \
        using CallbackName##Callback = std::pair<void *, CallbackName##CallbackT>; \
        \
        void connect##CallbackName(void *userData, \
                                   CallbackName##CallbackT callback) \
        { \
            if (!callback) \
                return; \
            \
            for (auto &func: this->m_##CallbackName##Callback) \
                if (func.first == userData \
                    && func.second == callback) \
                    return; \
            \
            this->m_##CallbackName##Callback.push_back({userData, callback});\
        } \
        \
        void disconnect##CallbackName(void *userData, \
                                      CallbackName##CallbackT callback) \
        { \
            if (!callback) \
                return; \
            \
            for (auto it = this->m_##CallbackName##Callback.begin(); \
                it != this->m_##CallbackName##Callback.end(); \
                it++) \
                if (it->first == userData \
                    && it->second == callback) { \
                    this->m_##CallbackName##Callback.erase(it); \
                    \
                    return; \
                } \
        } \
    \
    private: \
        std::vector<CallbackName##Callback> m_##CallbackName##Callback;

#define EMIT(owner, CallbackName, ...) \
    for (auto &callback: owner->m_##CallbackName##Callback) \
        if (callback.second) \
            callback.second(callback.first, __VA_ARGS__); \

#endif