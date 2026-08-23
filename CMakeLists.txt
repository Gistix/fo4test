cmake_minimum_required(VERSION 3.21)

project(
	Raytracing
	VERSION 0.0.1
	LANGUAGES CXX
)

# Make INSTALL depend on ALL build targets (default, but force it explicitly)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY OFF CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(DEPLOY_DIR $ENV{DEPLOY_DIR})

# ########################################################################################################################
# ## Build options
# ########################################################################################################################
message("Options:")

option(
    AUTO_DEPLOYMENT
    "Copy the build output and addons to env:DEPLOY_DIR"
    OFF
)

include(XSEPlugin)

find_package(directxtk CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

set(NGX_SDK_ROOT "" CACHE PATH
	"Root directory of the externally provided NVIDIA NGX SDK")

find_path(NGX_SDK_INCLUDE_DIR
	nvsdk_ngx.h
	HINTS "${NGX_SDK_ROOT}/include"
	REQUIRED
)

find_library(NGX_SDK_LIBRARY
	NAMES nvsdk_ngx_d
	HINTS "${NGX_SDK_ROOT}/lib/Windows_x86_64"
	REQUIRED
)

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
	${SIMPLEINI_INCLUDE_DIRS}
	${NGX_SDK_INCLUDE_DIR}
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	debug ${CMAKE_CURRENT_SOURCE_DIR}/include/detours/Debug/detours.lib
	optimized ${CMAKE_CURRENT_SOURCE_DIR}/include/detours/Release/detours.lib
	Microsoft::DirectXTK
	Microsoft::DirectXTex
	Microsoft::DirectX-Headers
	d3d12.lib
	magic_enum::magic_enum
	d3dcompiler.lib
	${NGX_SDK_LIBRARY}
)

if(AUTO_DEPLOYMENT AND DEPLOY_DIR)
    message(STATUS "Auto deployment enabled. Build output and addons will be copied to ${DEPLOY_DIR}")

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${DEPLOY_DIR}/${ScriptExtenderName}/Plugins)

    install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${DEPLOY_DIR}/${ScriptExtenderName}/Plugins OPTIONAL)

    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION ${DEPLOY_DIR}/${ScriptExtenderName}/Plugins OPTIONAL)

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/package/ DESTINATION ${DEPLOY_DIR})
endif()
