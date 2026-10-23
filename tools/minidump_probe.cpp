#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "Dbghelp.lib")

static const BYTE* Rva(const BYTE* base, ULONG32 rva)
{
    return base + rva;
}

static void PrintContext(const CONTEXT& c)
{
#if defined(_M_IX86)
    std::printf("EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\n",
        c.Eax, c.Ebx, c.Ecx, c.Edx);
    std::printf("ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX\n",
        c.Esi, c.Edi, c.Ebp, c.Esp);
    std::printf("EIP=%08lX EFLAGS=%08lX\n", c.Eip, c.EFlags);
#else
    std::printf("This probe must be built as x86 for an x86 dump.\n");
#endif
}

static bool ReadDumpMemory(const BYTE* base, const MINIDUMP_MEMORY64_LIST* list,
    ULONG64 address, void* output, size_t size)
{
    ULONG64 fileOffset = list->BaseRva;
    for (ULONG64 i = 0; i < list->NumberOfMemoryRanges; ++i)
    {
        const auto& range = list->MemoryRanges[i];
        const ULONG64 start = range.StartOfMemoryRange;
        const ULONG64 length = range.DataSize;
        if (address >= start && size <= length && address - start <= length - size)
        {
            memcpy(output, base + fileOffset + (address - start), size);
            return true;
        }
        fileOffset += length;
    }
    return false;
}

static void PrintMemory(const BYTE* base, const MINIDUMP_MEMORY64_LIST* list,
    ULONG64 address, size_t size)
{
    BYTE bytes[256] = {};
    if (size > sizeof(bytes))
        size = sizeof(bytes);
    if (!ReadDumpMemory(base, list, address, bytes, size))
    {
        std::printf("Memory %016llX size=%zu NOT_FOUND\n",
            static_cast<unsigned long long>(address), size);
        return;
    }
    std::printf("Memory %016llX size=%zu:\n",
        static_cast<unsigned long long>(address), size);
    for (size_t i = 0; i < size; i += 16)
    {
        std::printf("  %08llX: ", static_cast<unsigned long long>(address + i));
        const size_t row = (size - i < 16) ? size - i : 16;
        for (size_t j = 0; j < row; ++j)
            std::printf("%02X ", bytes[i + j]);
        std::printf("\n");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        fwprintf(stderr, L"usage: minidump_probe.exe <dump>\n");
        return 2;
    }

    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        fwprintf(stderr, L"CreateFileW failed: %lu\n", GetLastError());
        return 3;
    }
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping)
    {
        fwprintf(stderr, L"CreateFileMappingW failed: %lu\n", GetLastError());
        CloseHandle(file);
        return 4;
    }
    const BYTE* base = static_cast<const BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    if (!base)
    {
        fwprintf(stderr, L"MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(mapping);
        CloseHandle(file);
        return 5;
    }

    MINIDUMP_DIRECTORY* directory = nullptr;
    PVOID stream = nullptr;
    ULONG streamSize = 0;
    ULONG64 exceptionAddress = 0;
    ULONG64 stackPointer = 0;
    if (MiniDumpReadDumpStream(const_cast<BYTE*>(base), ExceptionStream,
        &directory, &stream, &streamSize))
    {
        const auto* exception = static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
        const auto& record = exception->ExceptionRecord;
        exceptionAddress = static_cast<ULONG64>(record.ExceptionAddress);
        std::printf("ExceptionThreadId=%lu\n", exception->ThreadId);
        std::printf("ExceptionCode=%08lX Flags=%08lX Address=%016llX Parameters=%lu\n",
            record.ExceptionCode, record.ExceptionFlags,
            static_cast<unsigned long long>(record.ExceptionAddress),
            record.NumberParameters);
        for (ULONG i = 0; i < record.NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
            std::printf("ExceptionParameter[%lu]=%016llX\n", i,
                static_cast<unsigned long long>(record.ExceptionInformation[i]));
        const auto* context = reinterpret_cast<const CONTEXT*>(Rva(base,
            exception->ThreadContext.Rva));
        PrintContext(*context);
#if defined(_M_IX86)
        stackPointer = context->Esp;
#endif
    }
    else
    {
        std::printf("ExceptionStream=NOT_FOUND\n");
    }

    const MINIDUMP_MEMORY64_LIST* memory64 = nullptr;
    if (MiniDumpReadDumpStream(const_cast<BYTE*>(base), Memory64ListStream,
        &directory, &stream, &streamSize))
    {
        memory64 = static_cast<const MINIDUMP_MEMORY64_LIST*>(stream);
        PrintMemory(base, memory64, exceptionAddress >= 32 ? exceptionAddress - 32 : 0, 96);
        PrintMemory(base, memory64, stackPointer, 256);
        for (int i = 2; i < argc; ++i)
        {
            const ULONG64 address = _wcstoui64(argv[i], nullptr, 0);
            PrintMemory(base, memory64, address, 64);
        }
    }
    else
    {
        std::printf("Memory64ListStream=NOT_FOUND\n");
    }

    if (MiniDumpReadDumpStream(const_cast<BYTE*>(base), ModuleListStream,
        &directory, &stream, &streamSize))
    {
        const auto* modules = static_cast<const MINIDUMP_MODULE_LIST*>(stream);
        std::printf("ModuleCount=%lu\n", modules->NumberOfModules);
        uintptr_t moduleExceptionAddress = 0;
        if (MiniDumpReadDumpStream(const_cast<BYTE*>(base), ExceptionStream,
            &directory, &stream, &streamSize))
        {
            const auto* exception = static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
            moduleExceptionAddress = static_cast<uintptr_t>(exception->ExceptionRecord.ExceptionAddress);
        }
        for (ULONG32 i = 0; i < modules->NumberOfModules; ++i)
        {
            const auto& module = modules->Modules[i];
            const auto* name = reinterpret_cast<const wchar_t*>(Rva(base,
                module.ModuleNameRva));
            std::printf("Module[%lu] base=%08llX size=%08lX name=",
                i, static_cast<unsigned long long>(module.BaseOfImage),
                module.SizeOfImage);
            wprintf(L"%ls", name);
            std::printf("\n");
            const uintptr_t begin = static_cast<uintptr_t>(module.BaseOfImage);
            const uintptr_t end = begin + module.SizeOfImage;
            if (moduleExceptionAddress >= begin && moduleExceptionAddress < end)
                std::printf("ExceptionModuleRva=%08llX\n",
                    static_cast<unsigned long long>(moduleExceptionAddress - begin));
        }
    }
    else
    {
        std::printf("ModuleListStream=NOT_FOUND\n");
    }

    UnmapViewOfFile(base);
    CloseHandle(mapping);
    CloseHandle(file);
    return 0;
}
