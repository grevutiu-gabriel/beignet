#
# Try to find X library and include path.
# Once done this will define
#
# DRM_INTEL_FOUND
# DRM_INTEL_INCLUDE_PATH
# 

FIND_LIBRARY(DRM_INTEL_LIBRARY
  NAMES DRM_INTEL drm_intel
  PATHS
  ~/lib/
  /usr/lib64
  /usr/lib
  /usr/local/lib64
  /usr/local/lib
  /sw/lib
  /opt/local/lib
  /usr/lib/i386-linux-gnu/
  DOC "The DRM_INTEL library")

IF(DRM_INTEL_INCLUDE_PATH)
  SET(DRM_INTEL_FOUND 1 CACHE STRING "Set to 1 if DRM_INTEL is found, 0 otherwise")
ELSE(DRM_INTEL_INCLUDE_PATH)
  SET(DRM_INTEL_FOUND 0 CACHE STRING "Set to 1 if DRM_INTEL is found, 0 otherwise")
ENDIF(DRM_INTEL_INCLUDE_PATH)

MARK_AS_ADVANCED(DRM_INTEL_FOUND)

