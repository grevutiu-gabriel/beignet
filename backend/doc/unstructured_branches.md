Unstructured Branches
=====================

A major challenge in making a OpenCL compiler is certainly to handle any kind of
branches. Indeed LLVM does not make any distinction between structured branches.
See [here](http://llvm.org/docs/LangRef.html) for a complete description of
the LLVM assembly specification.

The C branching code is simply lowered down in the following instructions:

- `ret` to return from the current function
- `br` that, if predicated, possibly jumps to two destinations (one for the
   taken branch and one for the other).
- `switch` that implements the C switch/case construct.
- `indirectbr` that implements a jump table
- `invoke` and `resume` mostly used to handle exceptions

Exceptions and jump tables are not supported in OpenCL. Switch cases can be
lowered down to a sequence of if/else statements (using a divide and conquer
approach a switch/case can be dispatched in log(n) complexity where n is the
number of targets).

This leads us to properly implement `br` and `ret` instructions.

Solution 1 - Using Gen structured branches
------------------------------------------

Gen structured branches are the following instructions:

`if` `else` `endif` `break` `continue` `while` `brd` `brc`

Transforming the LLVM IR code into structured code results in basically
reverse-engineering the LLVM code into the original C code.
Unfortunately, there are several key problems:

- OpenCL supports `goto` keyword that may jump to an arbitrary location
- LLVM can transform the control flow graph in any kind of form
- Worse is that a reducible control flow graph can be turned into an irreducible
one by the optimizer.

This can lead to complicated code transform and basic block duplication. The
specification allows the compiler to abort if an irreducible control flow is
detected but as an implementor, this is quite awkward to abort the compilation
because the optimizer turns an reducible CFG to an irreducible one. Using
structured branches is the open door to many corner cases.

Thing is it exists a pretty elegant solution that can be almost seamlessly
supported by Gen. This is the solution we retained.

Solution 2 - Linearizing the control flow graph
-----------------------------------------------

The general problem is to map a general control flow graph to a SIMD machine.
The problem is fairly well understood today. A recent research paper actually
dedicated to OpenCL like languages which use the "SPMD" (single program multiple
data) programming model present interesting insights about how to map SIMD
architectures to such languages (see [here]
(http://www.cdl.uni-saarland.de/papers/karrenberg_opencl.pdf)).

### Core idea

- Linearizing the CFG initially consists in removing all forward branches and
"replace" them by predication. Indeed, the program will be still correct if you
predicate instructions based instead of forward jumps. This is basically the
a control flow to data flow conversion.

- Of course, removing all forward branches is inefficient. To improve that, we
simply introduce "if conditions" in the head of basic blocks to know if we run
the basic block. If no lanes is going to be activated in the basic block, we
jump to another basic block where _potentially_ some lanes are going to be
reactivated.

Consider the following CFG:

<pre>
o-------o
|       |
|   1   |---->-----o
|       |          |
o-------o          |
    |              |
    |              |
o-------o          |
|       |          |
|   2   |---->-----------o
|       |          |     |
o-------o          |     |
    |              |     |
    |              |     |
    | o------o     |     |
    | |      |     |     |
    | v      |     |     |
o-------o    |     |     |
|       |    |     |     |
|   3   |    |     |     |
|       |    |     |     |
o-------o    |     |     |
    | |      |     |     |
    | o------o     |     |
    |              |     |
o-------o          |     |
|       |          |     |
|   4   |<---------o     |
|       |                |
o-------o                |
    |                    |
    |                    |
o-------o                |
|       |                |
|   5   |<----------------o
|       |
o-------o
</pre>

Mapping it to a SIMD machine may seem challenging. Actually it is not too
complicated. The problem is with the 2->5 jump. Indeed, we have to be sure that
we are not missing any computation done in block 4.

To do so:
- Instead of jumping from block 2 to block 5, we jump from block 2 to block 4. 
- We implement a `JOIN` point on top of block 4. We check if any lane is going
to be reactivated for the block 4. If not, we jump to block 5.

This leads to the following linearized CFG:
<pre>
o-------o
|       |
|   1   |---->-----o
|       |          |
o-------o          |
    |              |
    |              |
o-------o          |
|       |          |
|   2   |---->-----------o
|       |          |     |
o-------o          |     |
    |              |     |
    |              |     |
    | o--<---o     |     |
    | |      |     |     |
    | v      |     |     |
o-------o    |     |     |
|       |    |     |     |
|   3   |    ^     |     |
|       |    |     |     |
o-------o    |     |     |
    | |      |     |     |
    | o-->---o     |     |
    |              |     |
o-------o          |     |
|       |==========|=====|====O
|   4   |<---------|-----o    |
|       |<---------o          |
o-------o                     |
    |                         |
    |                         |
o-------o                     |
|       |                     |
|   5   |<====================O
|       |
o-------o
</pre>

There is a new jump from block 4 to block 5.

### Implementation on Gen

When using structured branches, Gen can supports auto-masking i.e. based on the
branches which are taken, the control flow is properly handled and masks are
automatically applied on all instructions.

However, there is no similar support for unstructured branches. We therefore
decided to mask instructions manually and use single program flow. This is
actually quite easy to do since Gen is able to predicate any branches.

Now, how to evaluate the if conditions in an efficient way?

The choice we did is to use *per-lane block IPs*: for each SIMD lane, we store a
short (16 bits) for each lane in a regular 256 bits GPR (general purpose
register). This "blockIP" register is used in the following way:

At the beginning of each block, we compare the blockIP register with the ID of
the block. The lane is going to be _activated_ if its blockIP is _smaller_ than
the ID of the block. Otherwise, the lane is deactivated.

Therefore, we build a flag register at the entry of each basic block with a
single 16-wide uint16_t compare. If no lane is activated, a jump is performed to
the next block where some lanes is going to be activated.

Since this is regular jumps, we just use `jmpi` instruction. With the help of
predication, we can express all the different possibilities:

- backward branches are always taken if _any_ of lanes in the predicate is true.
We just use `<+f0.0.anyh>` predication.
- forward branches is *not* taken if some of the lanes are going to activated in
the next block. We therefore compare the blockIP with the ID of the _next_
block. If all of them are strictly greater than the ID of the next block, we
jump. We therefore use the `<+f0.0.allh>` predicate in that case.
- `JOIN` points are even simpler. We simply jump if none of the lane is activated.
We therefore use the `<-f0.0.anyh>` predicate.

The complete encoding is done in `src/backend/gen_insn_selection.cpp`. Forward
branches are handled by `SimpleSelection::emitForwardBranch`. Backward branches
are handled by `SimpleSelection::emitBackwardBranch`. Finally, since `JOIN` points
are at the top of each basic blocks, they are handled by
`SimpleSelection::emitLabelInstruction`.

### Computing `JOIN` points

The last problem is to compute `JOIN` point i.e. we need to know if we need to
jump at the beginning of each block and if we do, what is the target of the
branch. The code is relatively straightforward and can be found in
`src/backend/context.cpp`. Function is `Context::buildJIPs`.
</br>
Actually, the current implementation is not that elegant. A colleague, Thomas
Raoux, has a simpler and better idea to handle it.

### Advantages and drawbacks of the method

- The method has one decisive advantage: it is simple and extremely robust. It can
handle any kind of CFGs (reducible or not) and does not require any
transformation. The use of shorts is also not random. 16-wide compares is issued
in 2 cycles (so it is twice fast as 16-wide 32 bits compares).
- Main drawback will be performance. Even if this is not so bad, we still need
more instructions than if we used structured branches. Mostly
  * one or two instructions for `JOIN` points
  * three instructions for backward and forward jumps (two more than structured
    branches that just require the branch instruction itself)

Note that all extra instructions are 16 bits instructions (i.e. they use shorts)
so they will only cost 2 cycles anyway.

The last point is that Gen encoding restricts conditional modifiers and
predicates to be the same in the instruction. This requires to copy or recompute
the flag register for compares and select. So one more instruction is required
for these two instructions. Once again, this would require only 2 cycles.

Remarks on `ret` instructions
-----------------------------

Since we can handle any kind of CFG, handling the return statements are
relatively straightforward. We first create one return block at the end of the
program. Then we replace all other returns by a unconditional jump to this
block. The CFG linearization will take care of the rest.
We then simply encode the (only one) return instruction as a End-Of-Thread
message (EOT).

Code examples
-------------

Some tests were written to assert the correctness of the CFG linearization and the
code generation. They can be found in the _run-time_ code base here:

`utest/compiler_if_else.cpp`

`utest/compiler_lower_return0.cpp`

`utest/compiler_lower_return1.cpp`

`utest/compiler_lower_return2.cpp`

`utest/compiler_short_scatter.cpp`

`utest/compiler_unstructured_branch0.cpp`

`utest/compiler_unstructured_branch1.cpp`

`utest/compiler_unstructured_branch2.cpp`

`utest/compiler_unstructured_branch3.cpp`

[Up](../README.html)

