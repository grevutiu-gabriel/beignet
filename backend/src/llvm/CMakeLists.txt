include (${LLVM_DIR}/AddLLVM.cmake)
include (${LLVM_DIR}/AddLLVMDefinitions.cmake)
include (${LLVM_DIR}/HandleLLVMOptions.cmake)
include (${LLVM_DIR}/LLVMProcessSources.cmake)
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(../)
add_llvm_target(GenBackend
  llvm_to_gen.cpp
  llvm_gen_backend.cpp
  llvm_passes.cpp)
target_link_libraries (LLVMGenBackend
  LLVMSupport
  LLVMAnalysis
  LLVMCodeGen
  LLVMCore
  LLVMScalarOpts
  LLVMTarget
  LLVMTransformUtils)

