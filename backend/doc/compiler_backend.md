Compiler Back End
=================

Well, the complete code base is somehow a compiler backend for LLVM. Here, we
really speak about the final code generation passes that you may find in
`src/backend`.

As explained in [the scalar IR presentation](./gen_ir.html), we bet on a very
simple scalar IR to make it easy to parse and modify. The idea is to fix the
unrelated problem (very Gen specific) where we can i.e. when the code is
generated.

The code generation in the compiler backend is classically divided into four
steps

- Instruction selection (defined in `src/backend/gen_insn_selection.*pp`). We
  expose an interface for the instruction selection engine. We implemented a
  very simple selection (called `SimpleSelection`) that does a quick and dirty
  one-to-many instruction generation.

- Register allocation (defined in `src/backend/gen_reg_allocation.*pp`). The
  code implements a linear scan allocator on the code selected in the previous
  pass. See below for more details about register vector allocations.

- Instruction scheduling. This one is not done yet. We just output the same
  instruction order as the program order. Note that we plan to implement an
  adaptive scheduling between register allocation and instruction  selection (to
  avoid spilling as much as possible)

- Instruction encoding. This is the final step that encodes the program into Gen
  ISA.

Instruction selection
---------------------

Usually, the instruction selection consists in mapping `p` instructions to `q`
ISA instructions under a cost driven model. Each basic block is therefore _tiled_
into some numbers of groups of ISA instructions such that the final cost is
minimized.

The literature is particularly dense on the subject. Compilers usually use today
either tree matching methods or selection DAG techniques (as LLVM backends do)

The instruction selection is still a work in progress in our compiler and we
only implement the most stupid (and inefficient) technique: we simply generate
as many instructions as we need for each _individual_ IR instructions. Since we
do not support immediate sources, this in particular leads to really ugly
looking code such as `mov (16) r2:f 1.f`. It is still a work in progress.

Other than that, the instruction selection is really a book keeping structure.
We basically output `SelectionInstruction` objects which are the 1-to-1 mapping
of Gen ISA encoding functions defined in `src/backend/gen_encoder.*pp`.

However, the `SelectionInstruction` still use unallocated virtual registers and
do *not* use vectors but simply tuples of virtual registers.

Register allocation
-------------------

The register allocation actually consists in two steps:

1. Handling the vector for all the instructions that require them

2. Performing the register allocation itself

Step 1 consists in scanning all the vectors required by sends. Obviously, the
same register may be used in different vectors and that may lead to
interferences. We simply sort the vectors from the largest to the smallest and
allocate them in that order. As an optimization we also identify sub-vectors
i.e. vectors included in larger ones and no not allocate them.

The code may be largely improved in particular if we take into account liveness
interferences as well. Basically, a register may be part of several vectors if the
registers that are not in both vectors at the same location are not alive at the
same time.

This is still a work in progress. Code is right now handled by method
`GenRegAllocator::allocateVector`.

Step 2 performs the register allocation i.e. it associates each virtual register
to one (or several) physical registers. The first thing is that the Gen register
file is very flexible i.e. it can (almost) be freely partitioned. To handle this
peculiarity, we simply implemented a free list based generic memory allocator as
done with `RegisterFilePartitioner` in `src/backend/context.cpp`.

We then simply implemented a linear scan allocator (see
`gen_reg_allocation.cpp`). The spilling is not implemented and is still a work
in progress. The thing is that spilling must be specifically handled with Gen.
Indeed:

1. Bad point. Spilling is expensive and require to assemble messages for it

2. Good point. Gen is able to spill up to 256 _contiguous_ bytes in one message.
This must be used for high performance spilling and this may require to reorder
properly registers to spill.

Instruction scheduling
----------------------

Intra-basic block instruction scheduling is relatively simple. It is not
implemented yet.

Instruction encoding
--------------------

This is mostly done in `src/backend/gen_context.cpp` and
`src/backend/gen_encoder./*pp`. This is mostly glue code and it is pretty
straightforward. We just forward the selection code using the physically
allocated registers. There is nothing special here. Just boilerplate.

[Up](../README.html)

