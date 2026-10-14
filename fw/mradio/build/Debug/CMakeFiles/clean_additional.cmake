# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "/home/nate/projects/mradio/fw/mradio/CM4/build"
  "/home/nate/projects/mradio/fw/mradio/CM7/build"
  )
endif()
