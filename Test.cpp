#include "Sockets.h"
#include "ClientSocket.h"
#include "ServerSocket.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include "IPv6.h"
#include "Assert.h"

using namespace std;

void test() {
	{
		ostringstream os;
		uint16_t ip[8] = { 0x2001, 0xdb8,0,0,1,0,0,1 };
		print_ipv6(os, ip, 0);
		assert_(strcmp(os.str().c_str(), "[2001:db8::1:0:0:1]:0") == 0);
	}
	{
		ostringstream os;
		uint16_t ip[8] = { 0, 0,0,0,0,0,0,1 };
		print_ipv6(os, ip, 123);
		assert_(strcmp(os.str().c_str(), "[::1]:123") == 0);
	}


	{ // Client test - HTTP GET google.co.uk
		Network::TCPClientSocket s("www.google.co.uk", 80, true);

		const char* hello = "GET /\r\n\r\n";
		s.send_data(hello, strlen(hello));

		char data[2000];

		for (int i = 0; i < 100; i++) {
			auto recvd = s.receive_data(data, 1999);

			if (recvd > 0) {
				data[recvd] = 0;
				cout << data << '\n';
			}
			else {
				if (recvd < 0) {
					cerr << "Error\n";
				}
				break;
			}
		}
	}

	struct MyClient : public Network::ServerSocket::Client {

	};


	Network::ServerIOManagerPoll io;
	auto s = io.add_tcp_socket(8000,
		// New connection
		[](Network::ServerSocket&, Network::IP ip, uint16_t clientPort) {
			return make_unique<MyClient>();
		},
		// Connection lost
		[](Network::ServerSocket&, Network::ServerSocket::Client&) {
			cout << "[conn lost]\n";
		},
		// Data recieved
		// It is safe to take io in the lambda as callbacks are only called from io.listen()
		[&io](Network::ServerSocket& socket, Network::ServerSocket::Client& client, Slice<const uint8_t> data) {
			cout << "Got data: " << data.ptr() << '\n';
			//const char* s = "cool beans";
			const char* s = "HTTP/1.1 200 OK\r\nConnection: Keep-alive\r\ncontent-length:2\r\n\r\nhi";
			io.send(socket, client, Slice(reinterpret_cast<const uint8_t *>(s), strlen(s)));
		},
		// Data sent
		[](Network::ServerSocket&, Network::ServerSocket::Client&, Slice<const uint8_t> data) {
		});


	while(true) {
		io.listen();
		this_thread::sleep_for(300ms);
	}
	cout << "Server closed\n";
}

int main() {
	Network::global_init();
	test();
	Network::global_deinit();
	return 0;
}