cmake_minimum_required(VERSION 3.7)

add_executable(datachannel-p2p main.cpp)

set_target_properties(datachannel-p2p PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME p2p)

set_target_properties(datachannel-p2p PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.p2p)

find_package(Threads REQUIRED)
target_link_libraries(datachannel-p2p LibDataChannel::LibDataChannel Threads::Threads)

if(MSVC)
	add_custom_command(TARGET datachannel-p2p POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE_DIR:datachannel>/datachannel.dll"
		$<TARGET_FILE_DIR:datachannel-media-sender>
	)
endif()
