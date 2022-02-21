CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

PROJECT(GPUReplay)

if(CMAKE_ENV STREQUAL "aarch64")
else()	# default: x86
endif()

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)

LINK_DIRECTORIES()

SET(CMAKE_CXX_COMPILER "g++")
SET(CMAKE_C_COMPILER "gcc")

#### common flags
SET(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wfatal-errors")
SET(CMAKE_C_FLAGS_DEGUG		"${CMAKE_C_FLAGS_DEBUG} -g -O0")
SET(CMAKE_C_FLAGS_RELEASE 	"${CMAKE_C_FLAGS_RELEASE} -O3")

# SET(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} -std=c+11")
# SET(CMAKE_CXX_FLAGS_DEBUG		"${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
# SET(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_DEBUG} -O3")
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=30)	# often 30
	# add_definitions(-D_GLIBCXX_DEBUG)      				# will fail compilation in folly
	add_definitions(-DDEBUG)
endif ()
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	add_definitions(-DNDEBUG)
endif ()


# add required libraries
INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/include/midgard
)

SET(SOURCE_COMMON
	gr_replay.c
	gr_mem.c
	gr_manager.c
	gr_utils.c
	gr_checkpoint.c
	gr_validation.c
	utils/measure.c
)

ADD_LIBRARY(lib_gr STATIC
	${SOURCE_COMMON}
)

############# NN inference ############

ADD_EXECUTABLE(test_mnist.bin
	test/test_mnist.c)
TARGET_LINK_LIBRARIES(test_mnist.bin
	lib_gr)

ADD_EXECUTABLE(test_alexnet.bin
	test/test_alexnet.c)
TARGET_LINK_LIBRARIES(test_alexnet.bin
	lib_gr)

ADD_EXECUTABLE(test_mobilenet.bin
	test/test_mobilenet.c)
TARGET_LINK_LIBRARIES(test_mobilenet.bin
	lib_gr)

ADD_EXECUTABLE(test_squeezenet.bin
	test/test_squeezenet.c)
TARGET_LINK_LIBRARIES(test_squeezenet.bin
	lib_gr)

ADD_EXECUTABLE(test_resnet12.bin
	test/test_resnet12.c)
TARGET_LINK_LIBRARIES(test_resnet12.bin
	lib_gr)

ADD_EXECUTABLE(test_vgg16.bin
	test/test_vgg16.c)
TARGET_LINK_LIBRARIES(test_vgg16.bin
	lib_gr)
