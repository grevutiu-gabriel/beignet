DECL_LLVM_GEN_FUNCTION(GET_GROUP_ID0, __gen_ocl_get_group_id0)
DECL_LLVM_GEN_FUNCTION(GET_GROUP_ID1, __gen_ocl_get_group_id1)
DECL_LLVM_GEN_FUNCTION(GET_GROUP_ID2, __gen_ocl_get_group_id2)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_ID0, __gen_ocl_get_local_id0)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_ID1, __gen_ocl_get_local_id1)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_ID2, __gen_ocl_get_local_id2)
DECL_LLVM_GEN_FUNCTION(GET_NUM_GROUPS0, __gen_ocl_get_num_groups0)
DECL_LLVM_GEN_FUNCTION(GET_NUM_GROUPS1, __gen_ocl_get_num_groups1)
DECL_LLVM_GEN_FUNCTION(GET_NUM_GROUPS2, __gen_ocl_get_num_groups2)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_SIZE0, __gen_ocl_get_local_size0)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_SIZE1, __gen_ocl_get_local_size1)
DECL_LLVM_GEN_FUNCTION(GET_LOCAL_SIZE2, __gen_ocl_get_local_size2)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_SIZE0, __gen_ocl_get_global_size0)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_SIZE1, __gen_ocl_get_global_size1)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_SIZE2, __gen_ocl_get_global_size2)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_OFFSET0, __gen_ocl_get_global_offset0)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_OFFSET1, __gen_ocl_get_global_offset1)
DECL_LLVM_GEN_FUNCTION(GET_GLOBAL_OFFSET2, __gen_ocl_get_global_offset2)

// Math function
DECL_LLVM_GEN_FUNCTION(ABS, __gen_ocl_fabs)
DECL_LLVM_GEN_FUNCTION(COS, __gen_ocl_cos)
DECL_LLVM_GEN_FUNCTION(SIN, __gen_ocl_sin)
DECL_LLVM_GEN_FUNCTION(SQR, __gen_ocl_sqrt)
DECL_LLVM_GEN_FUNCTION(RSQ, __gen_ocl_rsqrt)
DECL_LLVM_GEN_FUNCTION(LOG, __gen_ocl_log)
DECL_LLVM_GEN_FUNCTION(POW, __gen_ocl_pow)
DECL_LLVM_GEN_FUNCTION(RCP, __gen_ocl_rcp)
DECL_LLVM_GEN_FUNCTION(RNDZ, __gen_ocl_rndz)
DECL_LLVM_GEN_FUNCTION(RNDE, __gen_ocl_rnde)
DECL_LLVM_GEN_FUNCTION(RNDU, __gen_ocl_rndu)
DECL_LLVM_GEN_FUNCTION(RNDD, __gen_ocl_rndd)

// Barrier function
DECL_LLVM_GEN_FUNCTION(LBARRIER, __gen_ocl_barrier_local)
DECL_LLVM_GEN_FUNCTION(GBARRIER, __gen_ocl_barrier_global)
DECL_LLVM_GEN_FUNCTION(LGBARRIER, __gen_ocl_barrier_local_and_global)

// To force SIMD8/16 compilation
DECL_LLVM_GEN_FUNCTION(FORCE_SIMD8,  __gen_ocl_force_simd8)
DECL_LLVM_GEN_FUNCTION(FORCE_SIMD16, __gen_ocl_force_simd16)

