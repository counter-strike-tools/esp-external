#pragma once

#include <windows.h>

enum LogLevel {
    VERBOSE,
    INFO,
    WARNING,
    FATAL
};

#define LOGF(level, ...) do { (void)sizeof(level); } while (0)

class LogHelper {
public:
    static bool Init() { return true; }
    static void Destroy() {}
    static void Free()
    {
        if (HWND console = GetConsoleWindow()) {
            FreeConsole();
            PostMessage(console, WM_CLOSE, 0, 0);
        }
    }
};
