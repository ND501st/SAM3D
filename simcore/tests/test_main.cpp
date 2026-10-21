#include "test_framework.hpp"

#if defined(_MSC_VER) && defined(_DEBUG)
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#pragma comment(lib, "dbghelp.lib")

namespace {
// A crash in a debug run prints where it was (the stack, by symbol and line) instead of a dialog box.
void PrintStack(int) {
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);
    void* frames[48];
    const USHORT n = CaptureStackBackTrace(0, 48, frames, nullptr);
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    for (USHORT i = 0; i < n; ++i) {
        const DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(line);
        DWORD offset = 0;
        const bool named = SymFromAddr(process, address, nullptr, symbol) != FALSE;
        const bool lined = SymGetLineFromAddr64(process, address, &offset, &line) != FALSE;
        std::fprintf(stderr, "  %2u %s  %s:%lu\n", static_cast<unsigned>(i), named ? symbol->Name : "?", lined ? line.FileName : "?",
                     lined ? static_cast<unsigned long>(line.LineNumber) : 0ul);
    }
    std::fflush(stderr);
    std::_Exit(3);
}
}  // namespace
#endif

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    for (int kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
    }
    _set_abort_behavior(0, _CALL_REPORTFAULT);
    std::signal(SIGABRT, PrintStack);
#endif
    return sctest::RunAll(argc > 1 ? argv[1] : nullptr);
}
