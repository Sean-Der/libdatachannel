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



const int BUFFER_SIZE = 2048;
const int EXTENSION_HEADER_SIZE = 8;

void do_whip(std::shared_ptr<rtc::PeerConnection> pc) {
	int status;
	SOCKET client_fd;
	struct sockaddr_in serv_addr;
	char buffer[BUFFER_SIZE] = {0};
	struct addrinfo hints, *servinfo, *p;

	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		throw std::runtime_error("Failed to create socket for WHIP client");
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo("whip.siobud.com", "80", &hints, &servinfo) != 0) {
		throw std::runtime_error("Failed to resolve WHIP server");
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
		throw std::runtime_error("Failed to connect to WHIP server");
	}

	auto description = std::string(pc->localDescription().value());

	std::string request =
	    std::string("POST /api/whip HTTP/1.1\r\n") + "Host: whip.siobud.com\r\n" +
	    "Authorization: Bearer seanTest\r\n" + "Content-Type: application/sdp\r\n" +
	    "Content-Length: " + std::to_string(description.length()) + "\r\n\r\n" + description;
	if (send(client_fd, request.c_str(), strlen(request.c_str()), 0) == -1) {
		throw std::runtime_error("Failed to send offer to WHIP server");
	}

	if (read(client_fd, buffer, BUFFER_SIZE) == -1) {
		throw std::runtime_error("Failed to read answer from WHIP server");
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
		auto pc = std::make_shared<rtc::PeerConnection>();

		pc->onStateChange(
		    [](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });

		pc->onGatheringStateChange([pc](rtc::PeerConnection::GatheringState state) {
			std::cout << "Gathering State: " << state << std::endl;
			if (state == rtc::PeerConnection::GatheringState::Complete) {
				do_whip(pc);
			}
		});

		int sock = socket(AF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		addr.sin_port = htons(6000);

		if (bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0) {
			throw std::runtime_error("Failed to bind UDP socket on 127.0.0.1:6000");
		}

		int rcvBufSize = 212992;
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcvBufSize),
		           sizeof(rcvBufSize));

		auto midExtensionHeader = rtc::Description::Entry::ExtMap(1, "urn:ietf:params:rtp-hdrext:sdes:mid");
		auto ridExtensionHeader = rtc::Description::Entry::ExtMap(2, "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id");

		const rtc::SSRC highSsrc = 42;
		const rtc::SSRC medSsrc = 43;
		const rtc::SSRC lowSsrc = 44;
		const char mid[] = "0";
		const char highRid[] = "h";
		const char medRid[] = "m";
		const char lowRid[] = "l";

		rtc::Description::Video media("0", rtc::Description::Direction::SendOnly);
		media.addExtMap(midExtensionHeader);
		media.addExtMap(ridExtensionHeader);
		media.addH264Codec(96);
		media.addSSRC(highSsrc, "video-send", "video-send");
		media.addRid(highRid);
		media.addRid(medRid);
		media.addRid(lowRid);

		auto track = pc->addTrack(media);

		pc->setLocalDescription();

		char buffer[BUFFER_SIZE];
		int len;
		while ((len = recv(sock, buffer, BUFFER_SIZE, 0)) >= 0) {
			if (len < sizeof(rtc::RtpHeader) || !track->isOpen()) {
				continue;
			}

			auto rtp = reinterpret_cast<rtc::RtpHeader *>(buffer);

			// memmove to make room for extension headers
			std::memmove(rtp->getBody() + EXTENSION_HEADER_SIZE, rtp->getBody(), len);
			len += EXTENSION_HEADER_SIZE;
			rtp->setExtension(true);

			auto extHeader = rtp->getExtensionHeader();
			extHeader->setProfileSpecificId(0xbede);
			extHeader->setHeaderLength(1);
			extHeader->writeOneByteHeader(0, 1, reinterpret_cast<const std::byte *>(mid), 1);

			rtp->setSsrc(highSsrc);
			extHeader->writeOneByteHeader(2, 2, reinterpret_cast<const std::byte *>(highRid), 1);
			track->send(reinterpret_cast<const std::byte *>(buffer), len);

			rtp->setSsrc(medSsrc);
			extHeader->writeOneByteHeader(2, 2, reinterpret_cast<const std::byte *>(medRid), 1);
			track->send(reinterpret_cast<const std::byte *>(buffer), len);

			rtp->setSsrc(lowSsrc);
			extHeader->writeOneByteHeader(2, 2, reinterpret_cast<const std::byte *>(lowRid), 1);
			track->send(reinterpret_cast<const std::byte *>(buffer), len);
		}

	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
