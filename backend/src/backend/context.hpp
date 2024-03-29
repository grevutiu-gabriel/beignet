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

#ifndef __GBE_CONTEXT_HPP__
#define __GBE_CONTEXT_HPP__

#include "ir/instruction.hpp"
#include "backend/program.h"
#include "sys/set.hpp"
#include "sys/map.hpp"
#include "sys/platform.hpp"
#include <string>

namespace gbe {
namespace ir {

  class Unit;        // Contains the complete program
  class Function;    // We compile a function into a kernel
  class Liveness;    // Describes liveness of each ir function register
  class FunctionDAG; // Describes the instruction dependencies

} /* namespace ir */
} /* namespace gbe */

namespace gbe
{
  class Kernel;                 // context creates Kernel
  class RegisterFilePartitioner; // Partition register file for reg allocation

  /*! Context is the helper structure to build the Gen ISA or simulation code
   *  from GenIR
   */
  class Context : public NonCopyable
  {
  public:
    /*! Create a new context. name is the name of the function we want to
     *  compile
     */
    Context(const ir::Unit &unit, const std::string &name);
    /*! Release everything needed */
    virtual ~Context(void);
    /*! Compile the code */
    Kernel *compileKernel(void);
    /*! Tells if the labels is used */
    INLINE bool isLabelUsed(ir::LabelIndex index) const {
      return usedLabels.contains(index);
    }
    /*! Get the function graph */
    INLINE const ir::FunctionDAG &getFunctionDAG(void) const { return *dag; }
    /*! Get the liveness information */
    INLINE const ir::Liveness &getLiveness(void) const { return *liveness; }
    /*! Tells if the register is used */
    bool isRegUsed(const ir::Register &reg) const;
    /*! Indicate if a register is scalar or not */
    bool isScalarReg(const ir::Register &reg) const;
    /*! Get the kernel we are currently compiling */
    INLINE Kernel *getKernel(void) const { return this->kernel; }
    /*! Get the function we are currently compiling */
    INLINE const ir::Function &getFunction(void) const { return this->fn; }
    /*! Get the target label index for the given instruction */
    INLINE ir::LabelIndex getLabelIndex(const ir::Instruction *insn) const {
      GBE_ASSERT(JIPs.find(insn) != JIPs.end());
      return JIPs.find(insn)->second;
    }
    /*! Only GOTO and some LABEL instructions may have JIPs */
    INLINE bool hasJIP(const ir::Instruction *insn) const {
      return JIPs.find(insn) != JIPs.end();
    }
    /*! Allocate some memory in the register file */
    int16_t allocate(int16_t size, int16_t alignment);
    /*! Deallocate previously allocated memory */
    void deallocate(int16_t offset);
  protected:
    /*! Build the instruction stream. Return false if failed */
    virtual bool emitCode(void) = 0;
    /*! Allocate a new empty kernel (to be implemented) */
    virtual Kernel *allocateKernel(void) = 0;
    /*! Look if a stack is needed and allocate it */
    void buildStack(void);
    /*! Build the curbe patch list for the given kernel */
    void buildPatchList(void);
    /*! Build the list of arguments to set to launch the kernel */
    void buildArgList(void);
    /*! Build the sets of used labels */
    void buildUsedLabels(void);
    /*! Build JIPs for each branch and possibly labels. Can be different from
     *  the branch target due to unstructured branches
     */
    void buildJIPs(void);
    /*! Configure SLM use if needed */
    void handleSLM(void);
    /*! Insert a new entry with the given size in the Curbe. Return the offset
     *  of the entry
     */
    void newCurbeEntry(gbe_curbe_type value, uint32_t subValue, uint32_t size, uint32_t alignment = 0);
    /*! Provide for each branch and label the label index target */
    typedef map<const ir::Instruction*, ir::LabelIndex> JIPMap;
    const ir::Unit &unit;                 //!< Unit that contains the kernel
    const ir::Function &fn;               //!< Function to compile
    std::string name;                     //!< Name of the kernel to compile
    Kernel *kernel;                       //!< Kernel we are building
    ir::Liveness *liveness;               //!< Liveness info for the variables
    ir::FunctionDAG *dag;                 //!< Graph of values on the function
    RegisterFilePartitioner *partitioner; //!< Handle register file partionning
    set<ir::LabelIndex> usedLabels;       //!< Set of all used labels
    JIPMap JIPs;                          //!< Where to jump all labels/branches
    uint32_t simdWidth;                   //!< Number of lanes per HW threads
    GBE_CLASS(Context);                   //!< Use custom allocators
  };

} /* namespace gbe */

#endif /* __GBE_CONTEXT_HPP__ */

