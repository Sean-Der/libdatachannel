cmake_minimum_required(VERSION 3.7)

add_executable(datachannel-whep main.cpp)

set_target_properties(datachannel-whep PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME whep)

set_target_properties(datachannel-whep PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.whep)

find_package(Threads REQUIRED)
target_link_libraries(datachannel-whep LibDataChannel::LibDataChannel Threads::Threads)

if(MSVC)
	add_custom_command(TARGET datachannel-whep POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE_DIR:datachannel>/datachannel.dll"
		$<TARGET_FILE_DIR:datachannel-media-sender>
	)
endif()
