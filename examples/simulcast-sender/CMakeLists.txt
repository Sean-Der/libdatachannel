cmake_minimum_required(VERSION 3.7)

add_executable(datachannel-simulcast-sender main.cpp)

set_target_properties(datachannel-simulcast-sender PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME simulcast-sender)

set_target_properties(datachannel-simulcast-sender PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.simulcast-sender)

find_package(Threads REQUIRED)
target_link_libraries(datachannel-simulcast-sender LibDataChannel::LibDataChannel Threads::Threads)

if(MSVC)
	add_custom_command(TARGET datachannel-simulcast-sender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE_DIR:datachannel>/datachannel.dll"
		$<TARGET_FILE_DIR:datachannel-media-sender>
	)
endif()
