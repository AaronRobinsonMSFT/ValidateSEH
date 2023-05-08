using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ManagedRunner
{
    internal unsafe class Program
    {
        // User defined Structured Exception error code
        const uint TEST_SEH_ERROR = 0xE0001111;

        enum ExceptionType
        {
            Managed,
            Cpp,
            Structured
        };

        static delegate* unmanaged[Stdcall]<ExceptionType, char*, uint, void> _ThrowFromStack;
        static delegate* unmanaged[Stdcall]<ExceptionType, char*, uint, void> _ThrowFromHeap;
        static delegate* unmanaged[Stdcall]<nuint, delegate* unmanaged[Stdcall]<nuint, void>, void> _CreateManagedFrame;
        static delegate* unmanaged[Stdcall]<nuint, delegate* unmanaged[Stdcall]<nuint, void>, void> _CreateNativeFrame;

        static void Main(string[] args)
        {
            string path = @"SET_PATH_TO\CppCLI.dll";
            nint mod = NativeLibrary.Load(path);

            _ThrowFromStack = (delegate* unmanaged[Stdcall]<ExceptionType, char*, uint, void>)NativeLibrary.GetExport(mod, "ThrowFromStack");
            _ThrowFromHeap = (delegate* unmanaged[Stdcall]<ExceptionType, char*, uint, void>)NativeLibrary.GetExport(mod, "ThrowFromHeap");
            _CreateManagedFrame = (delegate* unmanaged[Stdcall]<nuint, delegate* unmanaged[Stdcall]<nuint, void>, void>)NativeLibrary.GetExport(mod, "CreateManagedFrame");
            _CreateNativeFrame = (delegate* unmanaged[Stdcall]<nuint, delegate* unmanaged[Stdcall]<nuint, void>, void>)NativeLibrary.GetExport(mod, "CreateNativeFrame");

            TestManaged();
            TestCpp();
            TestStructured();
        }

        sealed class DisposableManagedType : IDisposable
        {
            public nuint ID { get; init; }

            public void Dispose()
            {
                Console.WriteLine($"--- {ID} - {nameof(DisposableManagedType)}.{nameof(Dispose)}\n");
            }
        }

        sealed class TestCase
        {
            struct Context
            {
                public delegate* unmanaged[Stdcall]<ExceptionType, char*, uint, void> Thrower;
                public ExceptionType ExceptionType;
                public string Message;

                public void Throw()
                {
                    fixed (char* msg = Message)
                    {
                        Thrower(ExceptionType, msg, TEST_SEH_ERROR);
                    }
                }
            }

            [ThreadStatic]
            static Context s_cxt;

            readonly nuint _depth;
            readonly ExceptionType _type;
            public TestCase(string testName, int depth, ExceptionType type)
            {
                Console.WriteLine($"=== Running {testName} (depth: {depth})");
                _depth = (nuint)depth;
                _type = type;
            }

            public void Run(bool stack)
            {
                s_cxt = new Context()
                {
                    Thrower = stack ? _ThrowFromStack : _ThrowFromHeap,
                    ExceptionType = _type,
                    Message = $"Successfully caught {_type} exception",
                };
                Frame(_depth);
            }

            [MethodImpl(MethodImplOptions.NoInlining)]
            static void Frame(nuint depth)
            {
                if (depth == 0)
                {
                    s_cxt.Throw();
                }
                else if (depth % 2 == 0)
                {
                    _CreateNativeFrame(depth - 1, &FrameUnmanaged);
                }
                else
                {
                    using DisposableManagedType t = new() { ID = depth - 1 };
                    _CreateManagedFrame(t.ID, &FrameUnmanaged);
                }
            }

            [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
            static void FrameUnmanaged(nuint depth) => Frame(depth);
        }

        static void Fail()
        {
            Environment.FailFast("Failed to throw exception");
        }

        static void TestManaged()
        {
            TestCase s = new("Throw managed exception", 3, ExceptionType.Managed);
            try
            {
                s.Run(stack: true);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }

            TestCase h = new("Throw managed exception", 5, ExceptionType.Managed);
            try
            {
                h.Run(stack: false);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }
        }

        static void TestCpp()
        {
            TestCase s = new("Throw C++ exception", 3, ExceptionType.Cpp);
            try
            {
                s.Run(stack: true);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }

            TestCase h = new("Throw C++ exception", 5, ExceptionType.Cpp);
            try
            {
                h.Run(stack: false);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }
        }

        static void TestStructured()
        {
            TestCase s = new("Throw Structured exception", 3, ExceptionType.Structured);
            try
            {
                s.Run(stack: true);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }

            TestCase h = new("Throw Structured exception", 5, ExceptionType.Structured);
            try
            {
                h.Run(stack: false);
                Fail();
            }
            catch (Exception e)
            {
                Console.WriteLine($"{e}");
            }
        }
    }
}