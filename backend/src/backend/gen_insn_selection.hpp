/* 
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Segovia <benjamin.segovia@intel.com>
 */

/**
 * \file gen_insn_selection.hpp
 * \author Benjamin Segovia <benjamin.segovia@intel.com>
 */

#ifndef __GEN_INSN_SELECTION_HPP__
#define __GEN_INSN_SELECTION_HPP__

#include "ir/register.hpp"
#include "ir/instruction.hpp"
#include "backend/gen_register.hpp"
#include "backend/gen_encoder.hpp"
#include "backend/gen_context.hpp"
#include "sys/vector.hpp"
#include "sys/intrusive_list.hpp"

namespace gbe
{
  /*! Translate IR type to Gen type */
  uint32_t getGenType(ir::Type type);

  /*! Translate IR compare to Gen compare */
  uint32_t getGenCompare(ir::Opcode opcode);

  /*! Selection opcodes properly encoded from 0 to n for fast jump tables
   *  generations
   */
  enum SelectionOpcode {
#define DECL_SELECTION_IR(OP, FN) SEL_OP_##OP,
#include "backend/gen_insn_selection.hxx"
#undef DECL_SELECTION_IR
  };

  // Owns and Allocates selection instructions
  class Selection;

  // List of SelectionInstruction forms a block
  class SelectionBlock;

  /*! A selection instruction is also almost a Gen instruction but *before* the
   *  register allocation
   */
  class SelectionInstruction : public NonCopyable, public intrusive_list_node
  {
  public:
    /*! Owns the instruction */
    SelectionBlock *parent;
    /*! Append an instruction before this one */
    void prepend(SelectionInstruction &insn);
    /*! Append an instruction after this one */
    void append(SelectionInstruction &insn);
    /*! Does it read memory? */
    bool isRead(void) const;
    /*! Does it write memory? */
    bool isWrite(void) const;
    /*! Is it a branch instruction (i.e. modify control flow) */
    bool isBranch(void) const;
    /*! Is it a label instruction (i.e. change the implicit mask) */
    bool isLabel(void) const;
    /*! Get the destination register */
    GenRegister &dst(uint32_t dstID) { return regs[dstID]; }
    /*! Get the source register */
    GenRegister &src(uint32_t srcID) { return regs[dstNum+srcID]; }
    /*! Damn C++ */
    const GenRegister &dst(uint32_t dstID) const { return regs[dstID]; }
    /*! Damn C++ */
    const GenRegister &src(uint32_t srcID) const { return regs[dstNum+srcID]; }
    /*! No more than 6 sources (used by typed writes) */
    enum { MAX_SRC_NUM = 8 };
    /*! No more than 4 destinations (used by samples and untyped reads) */
    enum { MAX_DST_NUM = 4 };
    /*! State of the instruction (extra fields neeed for the encoding) */
    GenInstructionState state;
    union {
      struct {
        /*! Store bti for loads/stores and function for math and compares */
        uint16_t function:8;
        /*! elemSize for byte scatters / gathers, elemNum for untyped msg */
        uint16_t elem:8;
      };
      struct {
        /*! Number of sources in the tuple */
        uint16_t width:4;
        /*! vertical stride (0,1,2,4,8 or 16) */
        uint16_t vstride:5;
        /*! horizontal stride (0,1,2,4,8 or 16) */
        uint16_t hstride:5;
        /*! offset (0 to 7) */
        uint16_t offset:5;
      };
    } extra;
    /*! Gen opcode */
    uint8_t opcode;
    /*! Number of destinations */
    uint8_t dstNum:4;
    /*! Number of sources */
    uint8_t srcNum:4;
    /*! To store various indices */
    uint16_t index;
    /*! Variable sized. Destinations and sources go here */
    GenRegister regs[];
  private:
    /*! Just Selection class can create SelectionInstruction */
    SelectionInstruction(SelectionOpcode, uint32_t dstNum, uint32_t srcNum);
    // Allocates (with a linear allocator) and owns SelectionInstruction
    friend class Selection;
  };

  /*! Instructions like sends require to make registers contiguous in GRF */
  class SelectionVector : public NonCopyable, public intrusive_list_node
  {
  public:
    SelectionVector(void);
    /*! The instruction that requires the vector of registers */
    SelectionInstruction *insn;
    /*! Directly points to the selection instruction registers */
    GenRegister *reg;
    /*! Number of registers in the vector */
    uint16_t regNum;
    /*! Indicate if this a destination or a source vector */
    uint16_t isSrc;
  };

  // Owns the selection block
  class Selection;

  /*! A selection block is the counterpart of the IR Basic block. It contains
   *  the instructions generated from an IR basic block
   */
  class SelectionBlock : public NonCopyable, public intrusive_list_node
  {
  public:
    SelectionBlock(const ir::BasicBlock *bb);
    /*! All the emitted instructions in the block */
    intrusive_list<SelectionInstruction> insnList;
    /*! The vectors that may be required by some instructions of the block */
    intrusive_list<SelectionVector> vectorList;
    /*! Extra registers needed by the block (only live in the block) */
    gbe::vector<ir::Register> tmp;
    /*! Associated IR basic block */
    const ir::BasicBlock *bb;
    /*! Append a new temporary register */
    void append(ir::Register reg);
    /*! Append a new selection vector in the block */
    void append(SelectionVector *vec);
    /*! Append a new selection instruction at the end of the block */
    void append(SelectionInstruction *insn);
    /*! Append a new selection instruction at the beginning of the block */
    void prepend(SelectionInstruction *insn);
  };

  /*! Owns the selection engine */
  class GenContext;

  /*! Selection engine produces the pre-ISA instruction blocks */
  class Selection
  {
  public:
    /*! Initialize internal structures used for the selection */
    Selection(GenContext &ctx);
    /*! Release everything */
    ~Selection(void);
    /*! Implements the instruction selection itself */
    void select(void);
    /*! Bool and scalar register use scalar physical registers */
    bool isScalarOrBool(ir::Register reg) const;
    /*! Get the number of instructions of the largest block */
    uint32_t getLargestBlockSize(void) const;
    /*! Number of register vectors in the selection */
    uint32_t getVectorNum(void) const;
    /*! Number of registers (temporaries are created during selection) */
    uint32_t getRegNum(void) const;
    /*! Get the family for the given register */
    ir::RegisterFamily getRegisterFamily(ir::Register reg) const;
    /*! Get the data for the given register */
    ir::RegisterData getRegisterData(ir::Register reg) const;
    /*! Replace a source by the returned temporary register */
    ir::Register replaceSrc(SelectionInstruction *insn, uint32_t regID);
    /*! Replace a destination to the returned temporary register */
    ir::Register replaceDst(SelectionInstruction *insn, uint32_t regID);
    /*! Create a new selection instruction */
    SelectionInstruction *create(SelectionOpcode, uint32_t dstNum, uint32_t srcNum);
    /*! List of emitted blocks */
    intrusive_list<SelectionBlock> *blockList;
    /*! Actual implementation of the register allocator (use Pimpl) */
    class Opaque;
    /*! Created and destroyed in cpp */
    Opaque *opaque;
    /*! Use custom allocator */
    GBE_CLASS(Selection);
  };

} /* namespace gbe */

#endif /*  __GEN_INSN_SELECTION_HPP__ */

