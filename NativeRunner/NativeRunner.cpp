#define NOMINMAX
#include <Windows.h>

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <exception>

namespace
{
#define NOINLINE __declspec(noinline)
#define CCONV __stdcall

    // User defined Structured Exception error code
#define TEST_SEH_ERROR 0xE0001111

    enum class ExceptionType
    {
        Managed,
        Cpp,
        Structured
    };

    void(CCONV* _ThrowFromStack)(ExceptionType, WCHAR const*, DWORD);
    void(CCONV* _ThrowFromHeap)(ExceptionType, WCHAR const*, DWORD);
    void(CCONV* _CreateManagedFrame)(size_t id, void(CCONV *cb)(size_t id));
    void(CCONV* _CreateNativeFrame)(size_t id, void(CCONV *cb)(size_t id));
}

namespace
{
    struct ThrowCppException final
    {
        void operator()()
        {
            _ThrowFromStack(ExceptionType::Cpp, L"Successfully caught C++ exception", TEST_SEH_ERROR);
        }
    };

    struct ThrowStructuredException final
    {
        void operator()()
        {
            _ThrowFromStack(ExceptionType::Structured, L"Successfully caught structured exception (SEH)", TEST_SEH_ERROR);
        }
    };

    struct ThrowCppExceptionFromHeap final
    {
        void operator()()
        {
            _ThrowFromHeap(ExceptionType::Cpp, L"Successfully caught C++ exception", TEST_SEH_ERROR);
        }
    };

    struct ThrowStructuredExceptionFromHeap final
    {
        void operator()()
        {
            _ThrowFromHeap(ExceptionType::Structured, L"Successfully caught structured exception (SEH)", TEST_SEH_ERROR);
        }
    };

    template<typename OP>
    void NOINLINE CCONV Frame(size_t depth)
    {
        if (depth == 0)
        {
            OP op{};
            op();
        }
        else if (depth % 2 == 0)
        {
            _CreateNativeFrame(depth - 1, &Frame<OP>);
        }
        else
        {
            _CreateManagedFrame(depth - 1, &Frame<OP>);
        }
    }

    template<typename OP, size_t DEPTH>
    class TestCase final
    {
    public:
        TestCase(char const* testName) noexcept
        {
            std::printf("=== Running %s (depth: %zu)\n", testName, DEPTH);
        }
        void Run()
        {
            Frame<OP>(DEPTH);
        }
    };

    [[noreturn]]
    void Fail()
    {
        std::printf("Failed to throw exception\n");
        std::abort();
    }

    void TestCpp()
    {
        TestCase<ThrowCppException, 9> s{"Throw C++ exception"};
        try
        {
            s.Run();
            Fail();
        }
        catch (std::exception const& e)
        {
            std::printf("%s\n", e.what());
        }

        TestCase<ThrowCppExceptionFromHeap,11> h{"Throw C++ exception"};
        try
        {
            h.Run();
            Fail();
        }
        catch (std::exception const& e)
        {
            std::printf("%s\n", e.what());
        }
    }

    void TestStructured()
    {
        auto filter = [](uint32_t code, PEXCEPTION_POINTERS ep) -> uint32_t
        {
            if (code != TEST_SEH_ERROR)
                return EXCEPTION_CONTINUE_SEARCH;

            char const* msg = (char const*)ep->ExceptionRecord->ExceptionInformation[0];
            std::printf("%s\n", msg);

            return EXCEPTION_EXECUTE_HANDLER;
        };

        TestCase<ThrowStructuredException, 11> s{"Throw Structured exception"};
        __try
        {
            s.Run();
            Fail();
        }
        __except (filter(GetExceptionCode(), GetExceptionInformation()))
        {
            // Handled.
        }

        TestCase<ThrowStructuredExceptionFromHeap, 9> h{"Throw Structured exception"};
        __try
        {
            h.Run();
            Fail();
        }
        __except (filter(GetExceptionCode(), GetExceptionInformation()))
        {
            // Handled.
        }
    }
}

int main()
{
    LPCWSTR path = LR"**(.\CppCLI.dll)**";
    HMODULE mod = ::LoadLibraryW(path);
    if (mod == nullptr)
    {
        std::printf("Failed to load CppCLI.dll\n");
        return EXIT_FAILURE;
    }

    _ThrowFromStack = (decltype(_ThrowFromStack))::GetProcAddress(mod, "ThrowFromStack");
    _ThrowFromHeap = (decltype(_ThrowFromHeap))::GetProcAddress(mod, "ThrowFromHeap");
    _CreateManagedFrame = (decltype(_CreateManagedFrame))::GetProcAddress(mod, "CreateManagedFrame");
    _CreateNativeFrame = (decltype(_CreateNativeFrame))::GetProcAddress(mod, "CreateNativeFrame");
    if (_ThrowFromStack == nullptr
        || _ThrowFromHeap == nullptr
        || _CreateManagedFrame == nullptr
        || _CreateNativeFrame == nullptr)
    {
        std::printf("Failed to get exports from CppCLI.dll\n");
        return EXIT_FAILURE;
    }

    TestCpp();
    TestStructured();
}

