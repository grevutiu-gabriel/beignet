Flat Address Space
==================

Segmented address space...
--------------------------

The first challenge with OpenCL is its very liberal use of pointers. The memory
is segment into several address spaces:

- private. This is the memory for each work item

- global. These are buffers in memory shared by all work items and work groups

- constant. These are constant buffers in memory shared by all work items and
work groups as well

- local. These is a memory shared by all work items in the *same* work group

... But with no restriction inside each address space
-----------------------------------------------------

The challenge is that there is no restriction in OpenCL inside each address
space i.e. the full C semantic applies in particular regarding pointer
arithmetic.

Therefore the following code is valid:

<code>
\_\_kernel void example(\_\_global int *dst, \_\_global int *src0, \_\_global int *src1)<br/>
{<br/>
&nbsp;&nbsp;\_\_global int *from;<br/>
&nbsp;&nbsp;if (get\_global\_id(0) % 2)<br/>
&nbsp;&nbsp;&nbsp;&nbsp;from = src0;<br/>
&nbsp;&nbsp;else<br/>
&nbsp;&nbsp;&nbsp;&nbsp;from = src1;<br/>
&nbsp;&nbsp;dst[get\_global\_id(0)] = from[get\_global\_id(0)];<br/>
}
</code>

As one may see, the load done in the last line actually mixes pointers from both
source src0 and src1. This typically makes the use of binding table indices
pretty hard. In we use binding table 0 for dst, 1 for src0 and 2 for src1 (for
example), we are not able to express the load in the last line with one send
only.

No support for stateless in required messages
---------------------------------------------

Furthermore, in IVB, we are going four types of messages to implement the loads
and the stores

- Byte scattered reads. They are used to read bytes/shorts/integers that are not
aligned on 4 bytes. This is a gather message i.e. the user provides up to 16
addresses

- Byte scattered writes. They are used to write bytes/shorts/integers that are not
aligned on 4 bytes. This is a scatter message i.e. the user provides up to 16
addresses

- Untyped reads. They allow to read from 1 to 4 double words (i.e 4 bytes) per
lane. This is also a gather message i.e. up to 16 address are provided per
message.

- Untyped writes. They are the counter part of the untyped reads

Problem is that IVB does not support stateless accesses for these messages. So
surfaces are required. Secondly, stateless messages are not that interesting
since all of them require a header which is still slow to assemble.

Implemented solution
--------------------

The solution is actually quite simple. Even with no stateless support, it is
actually possible to simulate it with a surface. As one may see in the run-time
code in `intel/intel_gpgpu.c`, we simply create a surface:

- 2GB big

- Which starts at offset 0

Surprisingly, this surface can actually map the complete GTT address space which
is 2GB big. One may look at `flat_address_space` unit test in the run-time code
that creates and copies buffers in such a way that the complete GTT address
space is traversed.

This solution brings a pretty simple implementation in the compiler side.
Basically, there is nothing to do when translating from LLVM to Gen ISA. A
pointer to `__global` or `__constant` memory is simply a 32 bits offset in that
surface.

Related problems
----------------

There is one drawback for this approach. Since we use a 2GB surface that maps
the complete GTT space, there is no protection at all. Each write can therefore
potentially modify any buffer including the command buffer, the frame buffer or
the kernel code. There is *no* protection at all in the hardware to prevent
that.

[Up](../README.html)

