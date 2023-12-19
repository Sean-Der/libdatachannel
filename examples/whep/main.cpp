/**
 * libdatachannel simulcast sender example
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc/rtc.hpp"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WS2tcpip.h>
#include <io.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int SOCKET;
#endif

const int BUFFER_SIZE = 4096;
const int EXTENSION_HEADER_SIZE = 8;

void do_whep(std::shared_ptr<rtc::PeerConnection> pc) {
	SOCKET client_fd;
	char buffer[BUFFER_SIZE] = {0};
	struct addrinfo hints, *servinfo, *p;

	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		throw std::runtime_error("Failed to create socket for WHEP client");
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo("localhost", "8081", &hints, &servinfo) != 0) {
		throw std::runtime_error("Failed to resolve WHEP server");
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((client_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(client_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(client_fd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		throw std::runtime_error("Failed to connect to WHEP server");
	}

	auto description = std::string(pc->localDescription().value());

	std::string request =
	    std::string("POST /doSignaling HTTP/1.1\r\n") + "Host: localhost\r\n" +
	    "Authorization: Bearer seanTest\r\n" + "Content-Type: application/sdp\r\n" +
	    "Content-Length: " + std::to_string(description.length()) + "\r\n\r\n" + description;
	if (send(client_fd, request.c_str(), strlen(request.c_str()), 0) == -1) {
		throw std::runtime_error("Failed to send offer to WHEP server");
	}

	if (read(client_fd, buffer, BUFFER_SIZE) == -1) {
		throw std::runtime_error("Failed to read answer from WHEP server");
	}

	auto response = std::string(buffer);
	response.erase(0, response.find("v=0"));

	rtc::Description answer(response, "answer");
	pc->setRemoteDescription(answer);

	close(client_fd);
}

int main() {
	try {
		rtc::InitLogger(rtc::LogLevel::Debug);
		auto peer_connection = std::make_shared<rtc::PeerConnection>();

		peer_connection->onStateChange(
		    [](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });

		peer_connection->onGatheringStateChange(
		    [peer_connection](rtc::PeerConnection::GatheringState state) {
			    std::cout << "Gathering State: " << state << std::endl;
			    if (state == rtc::PeerConnection::GatheringState::Complete) {
				    do_whep(peer_connection);
			    }
		    });

		rtc::Description::Audio audioMedia("0", rtc::Description::Direction::RecvOnly);
		audioMedia.addOpusCodec(111);
		auto audio_track = peer_connection->addTrack(audioMedia);

		auto audio_session = std::make_shared<rtc::RtcpReceivingSession>();
		audio_track->setMediaHandler(audio_session);
		audio_track->onMessage([&](rtc::binary) {}, nullptr);

		rtc::Description::Video videoMedia("1", rtc::Description::Direction::RecvOnly);
		videoMedia.addH264Codec(96);
		auto video_track = peer_connection->addTrack(videoMedia);

		auto video_session = std::make_shared<rtc::RtcpReceivingSession>();
		auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>();
		video_session->addToChain(depacketizer);

		video_track->setMediaHandler(depacketizer);

		uint8_t hdr[] = {0x00, 0x00, 0x00, 0x01};
		auto file = fopen("out.h264", "w");

		video_track->onMessage(
		    [&](rtc::binary msg) {
			    fwrite(hdr, 1, 4, file);
			    fwrite(msg.data(), 1, msg.size(), file);
		    },
		    nullptr);

		peer_connection->setLocalDescription();

		std::promise<void>().get_future().wait();

	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
