#define NOMINMAX
#include <Windows.h>

#include <cstdio>
#include <string>
#include <stdexcept>
#include <memory>

//
// ISO C++ code
//
#pragma managed(push, off)
#define CCONV __stdcall

namespace
{
    std::string ConvertString(WCHAR const* str)
    {
        if (str == nullptr)
            return{};

        int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (len == 0)
            return{};

        auto buffer = std::make_unique<char[]>(len + 1);
        WideCharToMultiByte(CP_UTF8, 0, str, -1, buffer.get(), len + 1, nullptr, nullptr);
        return { buffer.get() };
    }
}
#pragma managed(pop)

//
// C++ CLI code
//
using namespace System;

namespace CppCLI
{
    ref class Test
    {
        String^ _managedString;
        int _errCode;
    public:
        Test(WCHAR const* msg, DWORD errCode)
            : _managedString(gcnew String(msg))
            , _errCode(errCode)
        { }

        ~Test()
        {
            std::printf("%s\n", __func__);
        }

        !Test()
        {
            std::printf("%s\n", __func__);
        }

        void ThrowManagedException()
        {
            throw gcnew Exception(_managedString);
        }

        void ThrowCppException(std::string const msg)
        {
            throw std::exception{ msg.c_str() };
        }

        void ThrowStructuredException(std::string const msg)
        {
            char const* msgRaw = msg.data();
            ::RaiseException(_errCode, 0, 1, (ULONG_PTR*)&msgRaw);
        }
    };

    ref struct ManagedStackType
    {
        size_t _id;
        ManagedStackType(size_t id)
            : _id(id)
        { }
        ~ManagedStackType()
        {
            std::printf("--- %4zu - %s\n", _id, __func__);
        }
    };

    enum class ExceptionType
    {
        Managed,
        Cpp,
        Structured
    };

#define NOINLINE __declspec(noinline)

    void NOINLINE ThrowFromStack(ExceptionType type, WCHAR const* str, DWORD errCode)
    {
        CppCLI::Test test(str, errCode);

        switch (type)
        {
        case ExceptionType::Managed:
            test.ThrowManagedException();
            break;
        case ExceptionType::Cpp:
            test.ThrowCppException(ConvertString(str));
            break;
        case ExceptionType::Structured:
            test.ThrowStructuredException(ConvertString(str));
            break;
        }
    }

    void NOINLINE ThrowFromHeap(ExceptionType type, WCHAR const* str, DWORD errCode)
    {
        CppCLI::Test^ test = gcnew CppCLI::Test(str, errCode);

        switch (type)
        {
        case ExceptionType::Managed:
            test->ThrowManagedException();
            break;
        case ExceptionType::Cpp:
            test->ThrowCppException(ConvertString(str));
            break;
        case ExceptionType::Structured:
            test->ThrowStructuredException(ConvertString(str));
            break;
        }
    }

    void NOINLINE CreateManagedFrame(size_t id, void(CCONV *cb)(size_t id))
    {
        ManagedStackType vc(id);
        cb(vc._id);
    }
}

//
// ISO C++ code
//
#pragma managed(push, off)
#define EXPORT __declspec(dllexport)

namespace
{
    struct UnmanagedStackType final
    {
        size_t _id;
        UnmanagedStackType(size_t id) noexcept
            : _id{ id }
        { }
        ~UnmanagedStackType() noexcept
        {
            std::printf("--- %4zu - %s\n", _id, __func__);
        }
    };
}

extern "C"
{
    void EXPORT CCONV ThrowFromStack(CppCLI::ExceptionType type, WCHAR const* str, DWORD errCode)
    {
        CppCLI::ThrowFromStack(type, str, errCode);
    }

    void EXPORT CCONV ThrowFromHeap(CppCLI::ExceptionType type, WCHAR const* str, DWORD errCode)
    {
        CppCLI::ThrowFromHeap(type, str, errCode);
    }

    void EXPORT CCONV CreateManagedFrame(size_t id, void(CCONV *cb)(size_t id))
    {
        CppCLI::CreateManagedFrame(id, cb);
    }

    void EXPORT CCONV CreateNativeFrame(size_t id, void(CCONV *cb)(size_t id))
    {
        UnmanagedStackType t{ id };
        cb(id);
    }
}
#pragma managed(pop)
