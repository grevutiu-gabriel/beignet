TODO
====

The compiler is far from complete. Even if the skeleton is now done and should
be solid, There are a _lot_ of things to do from trivial to complex.

OpenCL standard library
-----------------------

Today we define the OpenCL API in header file `src/ocl_stdlib.h`. This file is
from being complete.

By the way, one question remains: do we want to implement
the high-precision functions as _inline_ functions or as external functions to
call? Indeed, inlining all functions may lead to severe code bloats while
calling functions will require to implement a proper ABI. We certainly want to
do both actually.

LLVM front-end
--------------

The code is defined in `src/llvm`.  We used the PTX ABI and the OpenCL profile
to compile the code. Therefore, a good part of the job is already done. However,
many things must be implemented:

- Lowering down of various intrinsics like `llvm.memcpy`

- Implementation of most of the OpenCL built-ins (`native_cos`, `native_sin`,
  `mad`, atomic operations, barriers...)

- Lowering down of int16 / int8 / float16 / char16 / char8 / char4 loads and
  stores into the supported loads and stores

- Support for constant buffers declared in the OpenCL source file

- Support for local declaration of local array (the OpenCL profile will properly
  declare them as global arrays)

- Support for doubles

- Support for images. This will require to ensure that images are only directly
  accessed

- Better resolving of the PHI functions. Today, we always generate MOV
  instructions at the end of each basic block . They can be easily optimized.

Gen IR
------

The code is defined in `src/ir`. Main things to do are:

- Bringing support for doubles

- Adding proper support for SAMPLE and TYPED_WRITE instructions

- Adding support for BARRIER instructions

- Adding support for all the math instructions (native_cos, native_sin...)

- Finishing the handling of function arguments (see the [IR
  description](gen_ir.html) for more details)

- Adding support for constant data per unit

- Adding support for linking IR units together. OpenCL indeed allows to create
  programs from several sources

- Uniform analysys. This is a major performance improvement. A "uniform" value
  is basically a value where regardless the control flow, all the activated
  lanes will be identical. Trivial examples are immediate values, function
  arguments. Also, operations on uniform will produce uniform values and so
  on...

- Merging of independent uniform loads (and samples). This is a major
  performance improvement once the uniform analysis is done. Basically, several
  uniform loads may be collapsed into one load if no writes happens in-between.
  This will obviously impact both instruction selection and the register
  allocation.

Backend
-------

The code is defined in `src/backend`. Main things to do are:

- Bringing backend support for the missing instructions described above
  (native_sin, native_cos, barriers, samples...)

- Implementing support for doubles

- Implementing register spilling (see the [compiler backend
  description](./compiler_backend.html) for more details)

- Implementing proper instruction selection. A "simple" tree matching algorithm
  should provide good results for Gen

- Implementing the instruction scheduling pass

General plumbing
----------------

I tried to keep the code clean, well, as far as C++ can be really clean. There
are some header cleaning steps required though, in particular in the backend
code.

The context used in the IR code generation (see `src/ir/context.*pp`) should be
split up and cleaned up too.

I also purely and simply copied and pasted the Gen ISA disassembler from Mesa.
This leads to code duplication. Also some messages used by OpenCL (untyped reads
and writes) are not properly decoded yet.

There are some quick and dirty hacks also like the use of function call `system`
(...). This should be cleanly replaced by popen and stuff. I also directly
called the LLVM compiler executable instead of using Clang library. All of this
should be improved and cleaned up. Track "XXX" comments in the code.

Parts of the code leaks memory when exceptions are used. There are some pointers
to track and replace with std::unique_ptr. Note that we also add a custom memory
debugger that nicely complements (i.e. it is fast) Valgrind.

