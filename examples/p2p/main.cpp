/**
 * libdatachannel simulcast sender example
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc/rtc.hpp"
#include <memory>
#include <thread>

int main() {
	try {
		//rtc::InitLogger(rtc::LogLevel::Debug);
		auto offerer = std::make_shared<rtc::PeerConnection>();
		auto answerer = std::make_shared<rtc::PeerConnection>();

		offerer->onLocalDescription(
		    [&](rtc::Description description) { answerer->setRemoteDescription(description); });
		answerer->onLocalDescription(
		    [&](rtc::Description description) { offerer->setRemoteDescription(description); });

		offerer->onLocalCandidate(
		    [&](rtc::Candidate candidate) { answerer->addRemoteCandidate(candidate); });
		answerer->onLocalCandidate(
		    [&](rtc::Candidate candidate) { offerer->addRemoteCandidate(candidate); });

		auto datachannel = offerer->createDataChannel("test");
		datachannel->onOpen([&]() {
			std::cout << "[DataChannel open: " << datachannel->label() << "]" << std::endl;

			while (true) {
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				datachannel->send("Hello World");
			}
		});

		datachannel->onClosed([&]() {
			std::cout << "[DataChannel closed: " << datachannel->label() << "]" << std::endl;
		});

		std::shared_ptr<rtc::DataChannel> answererDataChannel;
		answerer->onDataChannel([&](std::shared_ptr<rtc::DataChannel> dc) {
			std::cout << "[Got a DataChannel with label: " << dc->label() << "]" << std::endl;
			dc->onClosed(
			    [&]() { std::cout << "[DataChannel closed: " << dc->label() << "]" << std::endl; });

			dc->onMessage([](auto data) {
				if (std::holds_alternative<std::string>(data)) {
					std::cout << "[Received message: " << std::get<std::string>(data) << "]"
					          << std::endl;
				}
			});

			answererDataChannel = dc;
		});

		offerer->setLocalDescription();
		std::promise<void>().get_future().wait();
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
