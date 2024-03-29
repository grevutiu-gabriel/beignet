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
 * \file gen_insn_scheduling.cpp
 * \author Benjamin Segovia <benjamin.segovia@intel.com>
 */

/*
 * Overall idea:
 * =============
 *
 * This is the instruction scheduling part of the code. With Gen, we actually
 * have a simple strategy to follow. Indeed, here are the constraints:
 *
 * 1 - the number of registers per HW thread is constant and given (128 32 bytes
 * GRF per thread). So, we can use all these registers with no penalty
 * 2 - spilling is super bad. Instruction latency matters but the top priority
 * is to avoid as much as possible spilling
 *
 *
 * We schedule twice using at each time a local forward list scheduler
 *
 * Before the register allocation
 * ==============================
 *
 * We try to limit the register pressure.
 * Well, this is a hard problem and we have a decent strategy now that we called
 * "zero cycled LIFO scheduling".
 * We use a local forward list scheduling and we schedule the instructions in a
 * LIFO order i.e. as a stack. Basically, we take the most recent instruction
 * and schedule it right away. Obviously we ignore completely the real latencies
 * and throuputs and just simulate instructions that are issued and completed in
 * zero cycle. For the complex kernels we already have (like menger sponge),
 * this provides a pretty good strategy enabling SIMD16 code generation where
 * when scheduling is deactivated, even SIMD8 fails
 *
 * One may argue that this strategy is bad, latency wise. This is not true since
 * the register allocator will anyway try to burn as many registers as possible.
 * So, there is still opportunities to schedule after register allocation.
 *
 * Our idea seems to work decently. There is however a strong research article
 * that is able to near-optimally reschudle the instructions to minimize
 * register use. This is:
 *
 * "Minimum Register Instruction Sequence Problem: Revisiting Optimal Code
 *  Generation for DAGs"
 *
 * After the register allocation
 * ==============================
 *
 * This is here a pretty simple strategy based on a regular forward list
 * scheduling. Since Gen is a co-issue based machine, this is useless to take
 * into account really precise timings since instruction issues will happen
 * out-of-order based on other thread executions.
 *
 * Note that we over-simplify the problem. Indeed, Gen register file is flexible
 * and we are able to use sub-registers of GRF in particular when we handle
 * uniforms or mask registers which are spilled in GRFs. Thing is that two
 * uniforms may not interfere even if they belong to the same GRF (i.e. they use
 * two different sub-registers). This means that the interference relation is
 * not transitive for Gen. To simplify everything, we just take consider full
 * GRFs (in SIMD8) or double full GRFs (in SIMD16) regardless of the fact this
 * is a uniform, a mask or a regular GRF.
 *
 * Obviously, this leads to extra dependencies in the code.
 */

#include "backend/gen_insn_selection.hpp"
#include "backend/gen_reg_allocation.hpp"
#include "sys/cvar.hpp"
#include "sys/intrusive_list.hpp"

namespace gbe
{
  // Helper structure to schedule the basic blocks
  struct SelectionScheduler;

  // Node for the schedule DAG
  struct ScheduleDAGNode;

  /*! We need to chain together the node we point */
  struct ScheduleListNode : public intrusive_list_node
  {
    INLINE ScheduleListNode(ScheduleDAGNode *node) : node(node) {}
    ScheduleDAGNode *node;
  };

  /*! Node of the DAG */
  struct ScheduleDAGNode
  {
    INLINE ScheduleDAGNode(SelectionInstruction &insn) :
      insn(insn), refNum(0), retiredCycle(0) {}
    bool dependsOn(ScheduleDAGNode *node) const {
      GBE_ASSERT(node != NULL);
      for (auto child : children)
        if (child.node == this)
          return true;
      return false;
    }
    /*! Children that depends on us */
    intrusive_list<ScheduleListNode> children;
    /*! Instruction after code selection */
    SelectionInstruction &insn;
    /*! Number of nodes that point to us (i.e. nodes we depend on) */
    uint32_t refNum;
    /*! Cycle when the instruction is retired */
    uint32_t retiredCycle;
  };

  /*! To track loads and stores */
  enum GenMemory : uint8_t {
    GLOBAL_MEMORY = 0,
    LOCAL_MEMORY,
    MAX_MEM_SYSTEM
  };

  /*! Do we allocate after or before the register allocation? */
  enum SchedulePolicy {
    PRE_ALLOC = 0, // LIFO scheduling (tends to limit register pressure)
    POST_ALLOC     // FIFO scheduling (limits latency problems)
  };

  /*! Helper structure to handle dependencies while scheduling. Takes into
   *  account virtual and physical registers and memory sub-systems
   */
  struct DependencyTracker : public NonCopyable
  {
    DependencyTracker(const Selection &selection, SelectionScheduler &scheduler);
    /*! Reset it before scheduling a new block */
    void clear(void);
    /*! Get an index in the node array for the given register */
    uint32_t getIndex(GenRegister reg) const;
    /*! Get an index in the node array for the given memory system */
    uint32_t getIndex(uint32_t bti) const;
    /*! Add a new dependency "node0 depends on node1" */
    void addDependency(ScheduleDAGNode *node0, ScheduleDAGNode *node1);
    /*! Add a new dependency "node0 depends on node located at index" */
    void addDependency(ScheduleDAGNode *node0, uint32_t index);
    /*! Add a new dependency "node located at index depends on node0" */
    void addDependency(uint32_t index, ScheduleDAGNode *node0);
    /*! No dependency for null registers and immediate */
    INLINE bool ignoreDependency(GenRegister reg) const {
      if (reg.file == GEN_IMMEDIATE_VALUE)
        return true;
      else if (reg.file == GEN_ARCHITECTURE_REGISTER_FILE) {
        if ((reg.nr & 0xf0) == GEN_ARF_NULL)
          return true;
      }
      return false;
    }
    /*! Add a new dependency "node0 depends on node set for register reg" */
    INLINE void addDependency(ScheduleDAGNode *node0, GenRegister reg) {
      if (this->ignoreDependency(reg) == false) {
        const uint32_t index = this->getIndex(reg);
        this->addDependency(node0, index);
      }
    }
    /*! Add a new dependency "node set for register reg depends on node0" */
    INLINE void addDependency(GenRegister reg, ScheduleDAGNode *node0) {
      if (this->ignoreDependency(reg) == false) {
        const uint32_t index = this->getIndex(reg);
        this->addDependency(index, node0);
      }
    }
    /*! Make the node located at insnID a barrier */
    void makeBarrier(int32_t insnID, int32_t insnNum);
    /*! Update all the writes (memory, predicates, registers) */
    void updateWrites(ScheduleDAGNode *node);
    /*! Maximum number of *physical* flag registers */
    static const uint32_t MAX_FLAG_REGISTER = 8u;
    /*! Maximum number of *physical* accumulators registers */
    static const uint32_t MAX_ACC_REGISTER = 1u;
    /*! Owns the tracker */
    SelectionScheduler &scheduler;
    /*! Stores the last node that wrote to a register / memory ... */
    vector<ScheduleDAGNode*> nodes;
    /*! Stores the nodes per instruction */
    vector<ScheduleDAGNode*> insnNodes;
    /*! Number of virtual register in the selection */
    uint32_t grfNum;
  };

  /*! Perform the instruction scheduling */
  struct SelectionScheduler : public NonCopyable
  {
    /*! Init the book keeping structures */
    SelectionScheduler(GenContext &ctx, Selection &selection, SchedulePolicy policy);
    /*! Make all lists empty */
    void clearLists(void);
    /*! Return the number of instructions to schedule in the DAG */
    int32_t buildDAG(SelectionBlock &bb);
    /*! Schedule the DAG */
    void scheduleDAG(SelectionBlock &bb, int32_t insnNum);
    /*! To limit register pressure or limit insn latency problems */
    SchedulePolicy policy;
    /*! Make ScheduleListNode allocation faster */
    DECL_POOL(ScheduleListNode, listPool);
    /*! Make ScheduleDAGNode allocation faster */
    DECL_POOL(ScheduleDAGNode, nodePool);
    /*! Ready list is instructions that can be scheduled */
    intrusive_list<ScheduleListNode> ready;
    /*! Active list is instructions that are executing */
    intrusive_list<ScheduleListNode> active;
    /*! Handle complete compilation */
    GenContext &ctx;
    /*! Code to schedule */
    Selection &selection;
    /*! To help tracking dependencies */
    DependencyTracker tracker;
  };

  DependencyTracker::DependencyTracker(const Selection &selection, SelectionScheduler &scheduler) :
    scheduler(scheduler)
  {
    if (scheduler.policy == PRE_ALLOC) {
      this->grfNum = selection.getRegNum();
      nodes.resize(grfNum + MAX_FLAG_REGISTER + MAX_ACC_REGISTER + MAX_MEM_SYSTEM);
    } else {
      const uint32_t simdWidth = scheduler.ctx.getSimdWidth();
      GBE_ASSERT(simdWidth == 8 || simdWidth == 16);
      this->grfNum = simdWidth == 8 ? 128 : 64;
      nodes.resize(grfNum + MAX_FLAG_REGISTER + MAX_ACC_REGISTER + MAX_MEM_SYSTEM);
    }
    insnNodes.resize(selection.getLargestBlockSize());
  }

  void DependencyTracker::clear(void) { for (auto &x : nodes) x = NULL; }

  void DependencyTracker::addDependency(ScheduleDAGNode *node0, ScheduleDAGNode *node1) {
    if (node0 != NULL && node1 != NULL && node0 != node1 && node0->dependsOn(node1) == false) {
      ScheduleListNode *dep = scheduler.newScheduleListNode(node0);
      node0->refNum++;
      node1->children.push_back(dep);
    }
  }

  void DependencyTracker::addDependency(ScheduleDAGNode *node, uint32_t index) {
    this->addDependency(node, this->nodes[index]);
  }

  void DependencyTracker::addDependency(uint32_t index, ScheduleDAGNode *node) {
    this->addDependency(this->nodes[index], node);
  }

  void DependencyTracker::makeBarrier(int32_t barrierID, int32_t insnNum) {
    ScheduleDAGNode *barrier = this->insnNodes[barrierID];

    // The barrier depends on all nodes before it
    for (int32_t insnID = 0; insnID < barrierID; ++insnID)
      this->addDependency(barrier, this->insnNodes[insnID]);

    // All nodes after barriers depend on the barrier
    for (int32_t insnID = barrierID + 1; insnID < insnNum; ++insnID)
      this->addDependency(this->insnNodes[insnID], barrier);
  }

  static GenRegister getFlag(const SelectionInstruction &insn) {
    if (insn.state.physicalFlag) {
      const uint32_t nr = insn.state.flag;
      const uint32_t subnr = insn.state.subFlag;
      return GenRegister::flag(nr, subnr);
    } else
      return GenRegister::uw1grf(ir::Register(insn.state.flagIndex));
  }

  uint32_t DependencyTracker::getIndex(GenRegister reg) const {
    // Non GRF physical register
    if (reg.physical) {
      GBE_ASSERT (reg.file == GEN_ARCHITECTURE_REGISTER_FILE);
      const uint32_t file = reg.nr & 0xf0;
      const uint32_t nr = reg.nr & 0x0f;
      if (file == GEN_ARF_FLAG) {
        const uint32_t subnr = reg.subnr / sizeof(uint16_t);
        GBE_ASSERT(nr < MAX_FLAG_REGISTER && (subnr == 0 || subnr == 1));
        return grfNum + 2*nr + subnr;
      } else if (file == GEN_ARF_ACCUMULATOR) {
        GBE_ASSERT(nr < MAX_ACC_REGISTER);
        return grfNum + MAX_FLAG_REGISTER + nr;
      } else {
        NOT_SUPPORTED;
        return 0;
      }
    }
    // We directly manipulate physical GRFs here
    else if (scheduler.policy == POST_ALLOC) {
      const GenRegister physical = scheduler.ctx.ra->genReg(reg);
      const uint32_t simdWidth = scheduler.ctx.getSimdWidth();
      return simdWidth == 8 ? physical.nr : physical.nr / 2;
    }
    // We use virtual registers since allocation is not done yet
    else 
      return reg.value.reg;
  }

  uint32_t DependencyTracker::getIndex(uint32_t bti) const {
    const uint32_t memDelta = grfNum + MAX_FLAG_REGISTER + MAX_ACC_REGISTER;
    return bti == 0xfe ? memDelta + LOCAL_MEMORY : memDelta + GLOBAL_MEMORY;
  }

  void DependencyTracker::updateWrites(ScheduleDAGNode *node) {
    const SelectionInstruction &insn = node->insn;

    // Track writes in registers
    for (uint32_t dstID = 0; dstID < insn.dstNum; ++dstID) {
      const GenRegister dst = insn.dst(dstID);
      if (this->ignoreDependency(dst) == false) {
        const uint32_t index = this->getIndex(dst);
        this->nodes[index] = node;
      }
    }

    // Track writes in predicates
    if (insn.opcode == SEL_OP_CMP) {
      const uint32_t index = this->getIndex(getFlag(insn));
      this->nodes[index] = node;
    }

    // Track writes in accumulators
    if (insn.state.accWrEnable) {
      const uint32_t index = this->getIndex(GenRegister::acc());
      this->nodes[index] = node;
    }

    // Track writes in memory
    if (insn.isWrite()) {
      const uint32_t index = this->getIndex(insn.extra.function);
      this->nodes[index] = node;
    }

    // Consider barriers and wait write to memory
    if (insn.opcode == SEL_OP_BARRIER || insn.opcode == SEL_OP_WAIT) {
      const uint32_t local = this->getIndex(0xfe);
      const uint32_t global = this->getIndex(0x00);
      this->nodes[local] = this->nodes[global] = node;
    }
  }

  /*! Kind-of roughly estimated latency. Nothing real here */
  static uint32_t getLatencyGen7(const SelectionInstruction &insn) {
#define DECL_GEN7_SCHEDULE(FAMILY, LATENCY, SIMD16, SIMD8)\
    const uint32_t FAMILY##InstructionLatency = LATENCY;
#include "gen_insn_gen7_schedule_info.hxx"
#undef DECL_GEN7_SCHEDULE

    switch (insn.opcode) {
#define DECL_SELECTION_IR(OP, FAMILY) case SEL_OP_##OP: return FAMILY##Latency;
#include "backend/gen_insn_selection.hxx"
#undef DECL_SELECTION_IR
    };
    return 0;
  }

  /*! Throughput in cycles for SIMD8 or SIMD16 */
  static uint32_t getThroughputGen7(const SelectionInstruction &insn, bool isSIMD8) {
#define DECL_GEN7_SCHEDULE(FAMILY, LATENCY, SIMD16, SIMD8)\
    const uint32_t FAMILY##InstructionThroughput = isSIMD8 ? SIMD8 : SIMD16;
#include "gen_insn_gen7_schedule_info.hxx"
#undef DECL_GEN7_SCHEDULE

    switch (insn.opcode) {
#define DECL_SELECTION_IR(OP, FAMILY) case SEL_OP_##OP: return FAMILY##Throughput;
#include "backend/gen_insn_selection.hxx"
#undef DECL_SELECTION_IR
    };
    return 0;
  }

  SelectionScheduler::SelectionScheduler(GenContext &ctx,
                                         Selection &selection,
                                         SchedulePolicy policy) :
    policy(policy), listPool(nextHighestPowerOf2(selection.getLargestBlockSize())),
    ctx(ctx), selection(selection), tracker(selection, *this)
  {
    this->clearLists();
  }

  void SelectionScheduler::clearLists(void) {
    this->ready.fast_clear();
    this->active.fast_clear();
  }

  int32_t SelectionScheduler::buildDAG(SelectionBlock &bb) {
    nodePool.rewind();
    listPool.rewind();
    tracker.clear();
    this->clearLists();

    // Track write-after-write and read-after-write dependencies
    int32_t insnNum = 0;
    for (auto &insn : bb.insnList) {
      // Create a new node for this instruction
      ScheduleDAGNode *node = this->newScheduleDAGNode(insn);
      tracker.insnNodes[insnNum++] = node;

      // read-after-write in registers
      for (uint32_t srcID = 0; srcID < insn.srcNum; ++srcID)
        tracker.addDependency(node, insn.src(srcID));

      // read-after-write for predicate
      if (insn.state.predicate != GEN_PREDICATE_NONE)
        tracker.addDependency(node, getFlag(insn));

      // read-after-write in memory
      if (insn.isRead()) {
        const uint32_t index = tracker.getIndex(insn.extra.function);
        tracker.addDependency(node, index);
      }

      // Consider barriers and wait are reading memory (local and global)
      if (insn.opcode == SEL_OP_BARRIER || insn.opcode == SEL_OP_WAIT) {
        const uint32_t local = tracker.getIndex(0xfe);
        const uint32_t global = tracker.getIndex(0x00);
        tracker.addDependency(node, local);
        tracker.addDependency(node, global);
      }

      // write-after-write in registers
      for (uint32_t dstID = 0; dstID < insn.dstNum; ++dstID)
        tracker.addDependency(node, insn.dst(dstID));

      // write-after-write for predicate
      if (insn.opcode == SEL_OP_CMP)
        tracker.addDependency(node, getFlag(insn));

      // write-after-write for accumulators
      if (insn.state.accWrEnable)
        tracker.addDependency(node, GenRegister::acc());

      // write-after-write in memory
      if (insn.isWrite()) {
        const uint32_t index = tracker.getIndex(insn.extra.function);
        tracker.addDependency(node, index);
      }

      // Consider barriers and wait are writing memory (local and global)
      if (insn.opcode == SEL_OP_BARRIER || insn.opcode == SEL_OP_WAIT) {
        const uint32_t local = tracker.getIndex(0xfe);
        const uint32_t global = tracker.getIndex(0x00);
        tracker.addDependency(node, local);
        tracker.addDependency(node, global);
      }

      // Track all writes done by the instruction
      tracker.updateWrites(node);
    }

    // Track write-after-read dependencies
    tracker.clear();
    for (int32_t insnID = insnNum-1; insnID >= 0; --insnID) {
      ScheduleDAGNode *node = tracker.insnNodes[insnID];
      const SelectionInstruction &insn = node->insn;

      // write-after-read in registers
      for (uint32_t srcID = 0; srcID < insn.srcNum; ++srcID)
        tracker.addDependency(insn.src(srcID), node);

      // write-after-read for predicate
      if (insn.state.predicate != GEN_PREDICATE_NONE)
        tracker.addDependency(getFlag(insn), node);

      // write-after-read in memory
      if (insn.isRead()) {
        const uint32_t index = tracker.getIndex(insn.extra.function);
        tracker.addDependency(index, node);
      }

      // Consider barriers and wait are reading memory (local and global)
      if (insn.opcode == SEL_OP_BARRIER || insn.opcode == SEL_OP_WAIT) {
        const uint32_t local = tracker.getIndex(0xfe);
        const uint32_t global = tracker.getIndex(0x00);
        tracker.addDependency(local, node);
        tracker.addDependency(global, node);
      }

      // Track all writes done by the instruction
      tracker.updateWrites(node);
    }

    // Make labels and branches non-schedulable (i.e. they act as barriers)
    for (int32_t insnID = 0; insnID < insnNum; ++insnID) {
      ScheduleDAGNode *node = tracker.insnNodes[insnID];
      if (node->insn.isBranch() || node->insn.isLabel() || node->insn.opcode == SEL_OP_EOT)
        tracker.makeBarrier(insnID, insnNum);
    }

    // Build the initial ready list (should only be the label actually)
    for (int32_t insnID = 0; insnID < insnNum; ++insnID) {
      ScheduleDAGNode *node = tracker.insnNodes[insnID];
      if (node->refNum == 0) {
        ScheduleListNode *listNode = this->newScheduleListNode(node);
        this->ready.push_back(listNode);
      }
    }

    return insnNum;
  }

  void SelectionScheduler::scheduleDAG(SelectionBlock &bb, int32_t insnNum) {
    uint32_t cycle = 0;
    const bool isSIMD8 = this->ctx.getSimdWidth() == 8;
    while (insnNum) {

      // Retire all the instructions that finished
      for (auto toRetireIt = active.begin(); toRetireIt != active.end();) {
        ScheduleDAGNode *toRetireNode = toRetireIt.node()->node;
        // Instruction is now complete
        if (toRetireNode->retiredCycle <= cycle) {
          toRetireIt = this->active.erase(toRetireIt);
          // Traverse all children and make them ready if no more dependency
          auto &children = toRetireNode->children;
          for (auto it = children.begin(); it != children.end();) {
            if (--it->node->refNum == 0) {
              ScheduleListNode *listNode = it.node();
              it = children.erase(it);
              this->ready.push_back(listNode);
            } else
              ++it;
          }
        }
        // Get the next one
        else
          ++toRetireIt;
      }

      // Try to schedule something from the ready list
      intrusive_list<ScheduleListNode>::iterator toSchedule;
      if (policy == POST_ALLOC) // FIFO scheduling
        toSchedule = this->ready.begin();
      else                      // LIFO scheduling
        toSchedule = this->ready.rbegin();
        // toSchedule = this->ready.begin();

      if (toSchedule != this->ready.end()) {
        // The instruction is instantaneously issued to simulate zero cycle
        // scheduling
        if (policy == POST_ALLOC)
          cycle += getThroughputGen7(toSchedule->node->insn, isSIMD8);

        this->ready.erase(toSchedule);
        this->active.push_back(toSchedule.node());
        // When we schedule before allocation, instruction is instantaneously
        // ready. This allows to have a real LIFO strategy
        if (policy == POST_ALLOC)
          toSchedule->node->retiredCycle = cycle + getLatencyGen7(toSchedule->node->insn);
        else
          toSchedule->node->retiredCycle = cycle;
        bb.append(&toSchedule->node->insn);
        insnNum--;
      } else
        cycle++;
    }
  }

  BVAR(OCL_POST_ALLOC_INSN_SCHEDULE, true);
  BVAR(OCL_PRE_ALLOC_INSN_SCHEDULE, true);

  void schedulePostRegAllocation(GenContext &ctx, Selection &selection) {
    if (OCL_POST_ALLOC_INSN_SCHEDULE) {
      SelectionScheduler scheduler(ctx, selection, POST_ALLOC);
      for (auto &bb : *selection.blockList) {
        const int32_t insnNum = scheduler.buildDAG(bb);
        bb.insnList.clear();
        scheduler.scheduleDAG(bb, insnNum);
      }
    }
  }

  void schedulePreRegAllocation(GenContext &ctx, Selection &selection) {
    if (OCL_PRE_ALLOC_INSN_SCHEDULE) {
      SelectionScheduler scheduler(ctx, selection, PRE_ALLOC);
      for (auto &bb : *selection.blockList) {
        const int32_t insnNum = scheduler.buildDAG(bb);
        bb.insnList.clear();
        scheduler.scheduleDAG(bb, insnNum);
      }
    }
  }

} /* namespace gbe */

