# List the source files
set(SRCS alpha.cpp
         clusterfit.cpp
         colourblock.cpp
         colourfit.cpp
         colourset.cpp
         maths.cpp
         rangefit.cpp
         singlecolourfit.cpp
         squish.cpp
)

# List the include paths
include_directories(.)

if(MSVC)
  set(variables CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
  foreach(variable ${variables})
    if(${variable} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
    endif()
  endforeach()
endif()

# Declaration of the library
add_library(squish STATIC ${SRCS})

# Compilation settings
if (NOT WIN32)
    set_target_properties(squish PROPERTIES COMPILE_FLAGS "-w -fPIC")
else()
    set_target_properties(squish PROPERTIES COMPILE_FLAGS "/W0")
endif()
