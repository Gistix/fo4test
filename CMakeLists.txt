cmake_minimum_required(VERSION 3.21)

project(
	ScreenspaceShadows
	VERSION 1.0.0
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XSEPlugin)

find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_library(DETOURS_LIBRARY detours REQUIRED)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${DETOURS_INCLUDE_DIRS}
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	${DETOURS_LIBRARY}
)

# https://gitlab.kitware.com/cmake/cmake/-/issues/24922#note_1371990
if(MSVC_VERSION GREATER_EQUAL 1936 AND MSVC_IDE) # 17.6+
	# When using /std:c++latest, "Build ISO C++23 Standard Library Modules" defaults to "Yes".
	# Default to "No" instead.
	#
	# As of CMake 3.26.4, there isn't a way to control this property
	# (https://gitlab.kitware.com/cmake/cmake/-/issues/24922),
	# We'll use the MSBuild project system instead
	# (https://learn.microsoft.com/en-us/cpp/build/reference/vcxproj-file-structure)
	file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Directory.Build.props" CONTENT [==[
<Project>
  <ItemDefinitionGroup>
    <ClCompile>
      <BuildStlModules>false</BuildStlModules>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
]==] @ONLY)
endif()