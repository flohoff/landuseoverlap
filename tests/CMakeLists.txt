find_package(Catch2 3 REQUIRED)

add_executable(unittests
	test_area.cpp test_areaindex.cpp test_reportstream.cpp test_writer.cpp)
target_include_directories(unittests PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(unittests overlapcore Catch2::Catch2WithMain)

add_test(NAME unittests COMMAND unittests)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure DEPENDS unittests)

# Coverage: -DENABLE_COVERAGE=ON; make coverage
if(ENABLE_COVERAGE)
	foreach(t overlapcore unittests)
		target_compile_options(${t} PRIVATE --coverage -O0 -g)
		target_link_options(${t} PRIVATE --coverage)
	endforeach()
	find_program(GCOVR gcovr REQUIRED)
	add_custom_target(coverage
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
		COMMAND $<TARGET_FILE:unittests>
		COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --filter ${CMAKE_SOURCE_DIR}/'[A-Za-z]*\\.[ch]pp'
			--exclude ${CMAKE_SOURCE_DIR}/landuseoverlap.cpp
			--txt --cobertura ${CMAKE_BINARY_DIR}/coverage/cobertura.xml --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
			--print-summary ${CMAKE_BINARY_DIR}
		DEPENDS unittests
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

# Mutation testing: CXX=clang++ cmake -DENABLE_MUTATION=ON; make mutation
if(ENABLE_MUTATION)
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "ENABLE_MUTATION needs clang++")
	endif()
	find_program(MULL_RUNNER NAMES mull-runner mull-runner-19 REQUIRED)
	find_file(MULL_PLUGIN NAMES mull-ir-frontend-19 PATHS /usr/lib /usr/local/lib REQUIRED)
	foreach(t overlapcore unittests)
		target_compile_options(${t} PRIVATE
			-fpass-plugin=${MULL_PLUGIN} -O0 -g -grecord-command-line)
	endforeach()
	# Only mutate project sources, not libosmium/gdalcpp headers or tests
	file(WRITE ${CMAKE_BINARY_DIR}/mull.yml
		"mutators:\n  - cxx_all\nincludePaths:\n  - ^${CMAKE_SOURCE_DIR}/[^/]*[.][ch]pp$\nexcludePaths:\n  - ^${CMAKE_SOURCE_DIR}/landuseoverlap[.]cpp$\n")
	add_custom_target(mutation
		COMMAND ${MULL_RUNNER} $<TARGET_FILE:unittests>
			--workers=4 --reporters=IDE --reporters=Elements
			--report-dir=${CMAKE_BINARY_DIR}/mutation
			--report-name=mutation
		DEPENDS unittests
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
