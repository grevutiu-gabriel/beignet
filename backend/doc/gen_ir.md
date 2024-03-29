Scalar Intermediate Representation
==================================

The IR code is included in `src/ir/` of the compiler code base
The IR as designed in this compiler is the fruit of a long reflection I mostly
have with Thomas Raoux. Note I usually call it "Gen IR".

Scalar vs vector IR
-------------------

This is actually the major question: do we need a vector IR or a scalar IR? On
the LLVM side, we have both. LLVM IR can manipulate vectors and scalars (and
even generalized values but we can ignore it for now).

For that reason, the Clang front-end generates both scalar and vector code.
Typically, a `uint4` variable will output a vector of 4 integers. Arithmetic
computations will be directly done on vector variables.

One the HW side, the situation is completely different:

- We are going to use the parallel mode (align1) i.e. the struct-of-array mode
  for the EU. This is a SIMD scalar mode.

- The only source of vectors we are going to have is on the sends instructions
  (and marginally for some other instructions like the div_rem math instruction)

One may therefore argue that we need vector instructions to handle the sends.
Send will indeed require both vector destinations and sources. This may be a
strong argument *for* vectors in the IR. However, the situation is not that
good.

Indeed, if we look carefully at the send instructions we see that they will
require vectors that are *not* vectors in LLVM IR. This code for example:

<code>
__global uint4 *src;<br/>
uint4 x = src[get\_global\_id(0)];<br/>
</code>

will be translated into an untyped write in the Gen ISA. Unfortunately, the
address and the values to write are in the *same* vector. However, LLVM IR will
output a store like:

`store(%addr, %value)`

which basically uses one scalar (the address) and one value (the vector to
write). Therefore even if we handle vectors in the IR, that will not directly
solve the problem we have at the end for the send instructions.

We therefore decided to go the other direction:

- We have a purely scalar IR

- To replace vectors, we simply use multiple sources and destinations

- Real vectors required by send instructions are handled at the very bottom of
the stack in the register allocation passes.

This leads to a very simple intermediate representation which is mostly a pure
scalar RISC machine.

Very limited IR
---------------

The other major question, in particular when you look similar stacks like NVidia
PTX, is:

do we need to encode in the IR register modifiers (abs, negate...) and immediate
registers (like in add.f x y 1.0)?

Contrary to other IRs (PTX and even LLVM that both supports immediates), we also
chose to have a very simply IR, much simpler than the final ISA, and to merge
back what we need at the instruction selection pass. Since we need instruction
selection, let us keep the IR simple.

Also, there are a lot of major issues that can not be covered in the IR and
require to be specifically handled at the very end of the code:

- send vectors (see previous section)

- send headers (value and register allocation) which are also part of the vector
problem

- SIMD8 mode in SIMD16 code. Some send messages do not support SIMD16 encoding
and require SIMD8. Typically examples are typed writes i.e. scatters to textures.
Also, this cannot be encoded in some way in a regular scalar IR.

For these reasons, most of the problems directly related to Gen naturally find
their solutions in either the instruction selection or the register allocator.

This leads to the following strategy:

- Keep the IR very simple and limited

- Use all the analysis tools you need in the IR before the final code generation
to build any information you need. This is pure "book-keeping".

- Use any previous analysis and finish the job at the very end

This classical approach leads to limit the complexity in the IR while forcing us
to write the proper tools in the final stages.

Why not using LLVM IR directly?
-------------------------------

We hesitated a long time between writing a dedicated IR (as we did) and just
using LLVM IR. Indeed, LLVM comes with a large set of tools that are parts of
"LLVM backends". LLVM provides a lot of tools to perform the instruction
selection (`SelectionDAG`) and the register allocation. Two things however
prevent us from choosing this path:

- We only have a limited experience with LLVM and no experience at all with the
LLVM backends

- LLVM register allocators do not handle at all the peculiarities of Gen:

  * flexible register file. Gen registers are more like memory than registers
    and can be freely allocated and aliased. LLVM register allocators only
    support partial aliasing like x86 machines do (rax -> eax -> ax)

  * no proper tools to handle vectors in the register allocator as we need for
    sends

Since we will need to do some significant work anyway, this leads us to choose a
more hard-coded path with a in-house IR. Note that will not prevent us from
implementing later a LLVM backend "by the book" as Nvidia does today with PTX
(using a LLVM backend to do the LLVM IR -> PTX conversion)

SSA or no SSA
-------------

Since we have a purely scalar IR, implementing a SSA transformation on the IR
may be convenient. However, most the literature about compiler back-ends use
non-SSA representation of the code. Since the primary goal is to write a
compiler _back-end_ (instruction selection, register allocation and instruction
scheduling), we keep the code in non-SSA letting the higher level optimizations
to LLVM.

Types, registers, instructions, functions and units
---------------------------------------------------

The IR is organized as follows:

- Types (defined in `src/ir/type.*pp`). These are scalar types only. Since the
  code is completely lowered down, there is no more reference to structures,
  pointers or vectors. Everything is scalar values and when "vectors" or
  "structures" would be needed, we use instead multiple scalar sources or
  destinations.

- Registers (defined in `src/ir/register.*pp`). They are untyped (since Gen IR
  are untyped) and we have 65,535 of them per function

- Instructions (defined in `src/ir/instruction.*pp`). They are typed (to
  distinguish integer and FP adds for example) and possibly support multiple
  destinations and sources. We also provide a convenient framework to introspect
  the instruction in a simple (and memory efficient) way

- Functions (defined in `src/ir/function.*pp`). They are basically the counter
  part of LLVM functions or OpenCL kernels. Note that function arguments are a
  problem. We actually use the PTX ABI. Everything smaller than the machine word
  size (i.e. 32 bits for Gen) is passed by value with a register. Everything
  else which is bigger than is passed by pointer with a ByVal attribute.
  Note that requires some special treatment in the IR (see below) to make the
  code faster by replacing function argument loads by  "pushed constants". We
  also defined one "register file" per function i.e. the registers are defined
  relatively to the function that uses them. Each function is made of basic
  blocks i.e. sequence of instructions that are executed linearly.

- Units (defined in `src/ir/unit.*pp`). Units are just a collection of
  functions and constants (not supported yet).

Function arguments and pushed constants
---------------------------------------

Gen can push values into the register file i.e. some registers are preset when
the kernel starts to run. As detailed previously, the PTX ABI is convenient
since every argument is either one register or one pointer to load from or to
store to.

However, when a pointer is used for an argument, loads are issued which may be
avoided by using constant pushes.

Once again OCL makes the task a bit harder than expected. Indeed, the C
semantic once again applies to function arguments as well.

Look at these three examples:

### Case 1. Direct loads -> constant push can be used

<code>
struct foo { int x; int y; }; </br>
\_\_kernel void case1(\_\_global int *dst, struct foo bar) </br>
{<br/>
&nbsp;&nbsp;dst[get\_global\_id(0)] = bar.x + bar.y;<br/>
}
</code>

We use a _direct_ _load_ for `bar` with `bar.x` and `bar.y`. Values can be
pushed into registers and we can replace the loads by register reads.

### Case 2. Indirect loads -> we need to load the values from memory

<code>
struct foo { int x[16]; }; </br>
\_\_kernel void case1(\_\_global int *dst, struct foo bar) </br>
{<br/>
&nbsp;&nbsp;dst[get\_global\_id(0)] = bar.x[get\_local\_id(0)];<br/>
}
</code>

We use an indirect load with `bar.x[get\_local\_id(0)]`. Here we need to issue a
load from memory (well, actually, we could do a gather from registers, but it is
not supported yet).

### Case 3. Writes to arguments -> we need to spill the values to memory first

<code>
struct foo { int x[16]; }; </br>
\_\_kernel void case1(\_\_global int *dst, struct foo bar) </br>
{<br/>
bar.x[0] = get\_global\_id(1);<br/>
&nbsp;&nbsp;dst[get\_global\_id(0)] = bar.x[get\_local\_id(0)];<br/>
}
</code>

Here the values are written before being read. This causes some troubles since
we are running in SIMD mode. Indeed, we only have in memory *one* instance of
the function arguments. Here, *many* SIMD lanes and actually *many* hardware
threads are running at the same time. This means that we can not write the data
to memory. We need to allocate a private area for each SIMD lane.

In that case, we need to spill back the function arguments into memory. We spill
once per SIMD lane. Then, we read from this private area rather than the
function arguments directly.

This analysis is partially done today in `src/ir/lowering.*pp`. We identify all
the cases but only the case with constant pushing is fully implemented.
Actually, the two last cases are easy to implement but this requires one or two
days of work.

Value and liveness analysis tools
---------------------------------

You may also notice that we provide a complete framework for value analysis
(i.e. to figure when a value or instruction destination is used and where the
instruction sources come from). The code is in `src/ir/value.*pp`. Well, today,
this code will burn a crazy amount of memory (use of std::set all over the
place) but it at least provides the analysis required by many other passes.
Compacting the data structures and using O(n) algorithms instead of the O(ln(n))
are in the TODO list for sure :-)

Finally, we also provide a liveness analysis tool which simply figures out which
registers are alive at the end of each block (classically "live out" sets).

[Up](../README.html)

