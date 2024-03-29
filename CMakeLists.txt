#############################################################################
#                  INTEL CORPORATION PROPRIETARY INFORMATION                #
#     This software is supplied under the terms of a license agreement or   #
#     nondisclosure agreement with Intel Corporation and may not be copied  #
#     or disclosed except in accordance with the terms of that agreement.   #
#          Copyright (C) 2009 Intel Corporation. All Rights Reserved.       #
#############################################################################

CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0)
PROJECT(OCL)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_VERBOSE_MAKEFILE "false")
SET(EMULATE_IVB false CACHE BOOL "To emulate IVB")
SET(EMULATE_SNB false CACHE BOOL "To emulate SNB")
SET(EMULATE_HSW false CACHE BOOL "To emulate HSW")
ADD_DEFINITIONS(-D__$(USER)__)

# Force Release with debug info
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)
set (CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "assure config" FORCE)
message(STATUS "Building mode: " ${CMAKE_BUILD_TYPE})

IF (EMULATE_HSW)
  SET (USE_FULSIM "true")
  ADD_DEFINITIONS(-DEMULATE_GEN=75)
ELSEIF (EMULATE_IVB)
  SET (USE_FULSIM "true")
  ADD_DEFINITIONS(-DEMULATE_GEN=7)
ELSEIF (EMULATE_SNB)
  SET (USE_FULSIM "true")
  ADD_DEFINITIONS(-DEMULATE_GEN=6)
ELSE (EMULATE_IVB)
  SET (USE_FULSIM "false")
  ADD_DEFINITIONS(-DEMULATE_GEN=0)
ENDIF (EMULATE_HSW)

IF (USE_FULSIM)
  ADD_DEFINITIONS(-DUSE_FULSIM=1)
ELSE (USE_FULSIM)
  ADD_DEFINITIONS(-DUSE_FULSIM=0)
ENDIF (USE_FULSIM)

SET(CMAKE_CXX_FLAGS "-Wall -Wno-invalid-offsetof -mfpmath=sse --no-rtti -Wcast-align -std=c++0x")
SET(CMAKE_C_FLAGS "-Wall -mfpmath=sse -msse2 -Wcast-align")

# Front end stuff we need
Find_Package(LLVM REQUIRED)

# XLib
INCLUDE(CMake/FindXLib.cmake)
IF(XLIB_FOUND)
  MESSAGE(STATUS "Looking for XLib - found")
ELSE(XLIB_FOUND)
  MESSAGE(STATUS "Looking for XLib - not found")
ENDIF(XLIB_FOUND)

# DRM
INCLUDE(CMake/FindDRM.cmake)
IF(DRM_FOUND)
  MESSAGE(STATUS "Looking for DRM - found")
ELSE(DRM_FOUND)
  MESSAGE(STATUS "Looking for DRM - not found")
ENDIF(DRM_FOUND)

# DRM Intel
INCLUDE(CMake/FindDRMIntel.cmake)
IF(DRM_INTEL_FOUND)
  MESSAGE(STATUS "Looking for DRM Intel - found")
ELSE(DRM_INTEL_FOUND)
  MESSAGE(STATUS "Looking for DRM Intel - not found")
ENDIF(DRM_INTEL_FOUND)

# Xext
INCLUDE(CMake/FindXext.cmake)
IF(XEXT_FOUND)
  MESSAGE(STATUS "Looking for Xext - found")
ELSE(XEXT_FOUND)
  MESSAGE(STATUS "Looking for Xext - not found")
ENDIF(XEXT_FOUND)

# Xfixes
INCLUDE(CMake/FindXfixes.cmake)
IF(XFIXES_FOUND)
  MESSAGE(STATUS "Looking for Xfixes - found")
ELSE(XFIXES_FOUND)
  MESSAGE(STATUS "Looking for Xfixes - not found")
ENDIF(XFIXES_FOUND)

# Gen-backend (compiler)
INCLUDE(CMake/FindGBE.cmake)
IF(GBE_FOUND)
  MESSAGE(STATUS "Looking for Gen-Backend - found")
ELSE(GBE_FOUND)
  MESSAGE(STATUS "Looking for Gen-Backend - not found")
ENDIF(GBE_FOUND)

ADD_SUBDIRECTORY(backend)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(utests)

