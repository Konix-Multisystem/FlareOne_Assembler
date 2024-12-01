cmake_minimum_required(VERSION 3.4.3)
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_C_STANDARD 99)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(assembler)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

BISON_TARGET(MyParser src/z80.y
	${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp)
FLEX_TARGET(MyLexer src/z80.l
	${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

set(HEADERS src/ast.h)
set(GRAMMARS src/z80.y src/z80.l)
set(CPPS src/main.cpp)

add_definitions(-DOS_WINDOWS=1)

add_executable(assembler 
	${CPPS}
	${GRAMMARS}
	${HEADERS}
	${FLEX_MyLexer_OUTPUTS}
	${BISON_MyParser_OUTPUTS} 
	)

target_include_directories(assembler PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(assembler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (WIN32)
	target_compile_definitions(assembler PUBLIC -DYY_NO_UNISTD_H)
endif()

source_group(Sources FILES ${CPPS})
source_group(Headers FILES ${HEADERS})
source_group(Grammar FILES ${GRAMMARS})
source_group(Generated FILES ${FLEX_MyLexer_OUTPUTS} ${BISON_MyParser_OUTPUTS})

#
# For debugging
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
