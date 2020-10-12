
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "Sockets.h"
#include "ServerSocket.h"
#include <stdexcept>
#include <string>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include "Assert.h"
#include "IPv6.h"

using namespace std;

thread_local uint8_t receive_buffer[128 * 1024];

namespace Network {

	shared_ptr<ServerSocket> ServerIOManagerPoll::add_tcp_socket(uint16_t port, ServerSocket::NewConnection newConn, ServerSocket::ConnectionLost connLost,
		ServerSocket::DataRecieved dataRecvd, ServerSocket::SendComplete sendDone) {

		int iResult;

		addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

		char portString[20];
		sprintf(portString, "%d", port);
		addrinfo* result = nullptr;
		iResult = getaddrinfo(nullptr, portString, &hints, &result);
		if (iResult != 0 || result == nullptr) {
			throw runtime_error(string("getaddrinfo error: ") + to_string(iResult));
		}

		// Loop over all addresses and create an IPv4 socket and an IPv6 socket

		auto address = result;

		SOCKET ipv4_socket = INVALID_SOCKET;
		SOCKET ipv6_socket = INVALID_SOCKET;

		while (address != nullptr) {
			const auto setup_socket = [](SOCKET& sock, int af, addrinfo* address) {
				sock = socket(af, SOCK_STREAM, IPPROTO_TCP);
				if (sock != INVALID_SOCKET) {
					if (::bind(sock, address->ai_addr, address->ai_addrlen) == SOCKET_ERROR) {
						closesocket(sock);
						sock = INVALID_SOCKET;
					}
					else {
						if (::listen(sock, SOMAXCONN) == SOCKET_ERROR) {
							closesocket(sock);
							sock = INVALID_SOCKET;
						}
						else {
							unsigned long x;
							ioctlsocket(sock, FIONBIO, &x);
						}
					}
				}
			};

			if (ipv4_socket == INVALID_SOCKET && address->ai_family == PF_INET) {
				// IPv4
				setup_socket(ipv4_socket, PF_INET, address);
			}
			else if (ipv6_socket == INVALID_SOCKET && address->ai_family == PF_INET6) {
				// IPv6
				setup_socket(ipv6_socket, PF_INET6, address);
			}

			if (ipv4_socket != INVALID_SOCKET && ipv6_socket != INVALID_SOCKET) {
				break;
			}

			address = address->ai_next;
		}


		freeaddrinfo(result);

		if (ipv4_socket == INVALID_SOCKET) {
			throw runtime_error("Socket creation/bind error");
		}

		optional<uintptr_t> ipv6Handle = nullopt;

		if (ipv6_socket == INVALID_SOCKET) {
			cerr << "Unable to create IPv6 socket\n";
		}
		else {
			ipv6Handle = ipv6_socket;
		}


		auto socket_ptr = make_shared<TCPServerSocket>(this, port, ipv4_socket, ipv6Handle, newConn, connLost, dataRecvd, sendDone);
		sockets.push_back(socket_ptr);
		return socket_ptr;
	}

	void ServerIOManagerPoll::close_socket(ServerSocket& socket) {
		closesocket(socket.ipv4Handle);

		auto v6 = socket.ipv6Handle;
		if (v6.has_value()) {
			closesocket(v6.value());
		}
	}

	ServerIOManagerPoll::ServerIOManagerPoll() {

	}

	/*void ServerIOManagerPoll::add_file(std::shared_ptr<File>&& file) {
		files.emplace_back(move(file));
	}*/

	void ServerIOManagerPoll::poll_tcp_socket(TCPServerSocket& socket, uintptr_t handle) {
		// Check for new connections

		sockaddr_in6 clientAddr;
		int clientAddrLen = sizeof(clientAddr);
		auto client_socket = accept(handle, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

		if (client_socket == INVALID_SOCKET) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				// Actual error
				throw runtime_error("accept error");
			}
		}
		else {
			// Client connected

			IP ip;
			uint16_t clientPort;

			if (clientAddr.sin6_family == AF_INET6) {
				memcpy(ip.v6, clientAddr.sin6_addr.u.Byte, 16);
				ip.isV6 = true;
				clientPort = clientAddr.sin6_port;


				clog << "Client connecting from: ";
				print_ipv6(clog, ip.v6_u16, clientPort);
				clog << '\n';

			}
			else {
				sockaddr_in* clientAddr4 = reinterpret_cast<sockaddr_in*>(&clientAddr);
				ip.v4 = clientAddr4->sin_addr.S_un.S_addr;
				ip.isV6 = false;
				clientPort = clientAddr4->sin_port;

				clog << "Client connecting from: " << (unsigned)ip.v4_array[0] << "." << (unsigned)ip.v4_array[1] << "." <<
					(unsigned)ip.v4_array[2] << "." << (unsigned)ip.v4_array[3] << ":" <<
					clientPort << '\n';
			}

			auto client = socket.newConn(socket, ip, clientPort);
			if (client) {
				client->ip = ip;
				client->clientPort = clientPort;
				client->handle = client_socket;
				socket.clients.emplace_back(move(client));
			}
		}

		// Check existing connections

		auto i = socket.clients.begin();
		while (i != socket.clients.end()) {
			auto& client = *i;

			int iResult = recv(client->handle, reinterpret_cast<char*>(receive_buffer), sizeof receive_buffer, 0);
			if (iResult <= 0) {
				if (iResult != 0) {
					auto e = WSAGetLastError();
					if (e == WSAEWOULDBLOCK) {
						continue;
					}
					clog << "Socket error: " << e << '\n';
				}
				socket.connLost(socket, *client);
				closesocket(client->handle);
				socket.clients.erase(i++);
				continue;
			}
			else if (iResult > 0) {
				// Data in
				receive_buffer[sizeof receive_buffer - 1] = 0; // TODO REMOVE THIS
				socket.dataRecvd(socket, *client, Slice<const uint8_t>(receive_buffer, sizeof receive_buffer));
			}
			++i;
		}
	}

	void ServerIOManagerPoll::poll_tcp_socket(TCPServerSocket& socket) {
		poll_tcp_socket(socket, socket.get_ipv4_handle());
		if (socket.get_ipv6_handle().has_value()) {
			poll_tcp_socket(socket, socket.get_ipv6_handle().value());
		}
	}

	void ServerIOManagerPoll::listen(uint32_t timeout) {
		listen();
	}


	void ServerIOManagerPoll::listen() {
		for (auto& socket : sockets) {
			if (socket->is_reliable()) {
				poll_tcp_socket(*dynamic_cast<TCPServerSocket*>(socket.get()));
			}
			else {
				// poll_udp_socket(*dynamic_cast<UDPServerSocket*>(socket.get()));
			}
		}
	}

	void ServerIOManagerPoll::send(ServerSocket& socket, ServerSocket::Client& client, Slice<const uint8_t> data) {
		int iResult = ::send(client.handle, reinterpret_cast<const char *>(data.ptr()), data.size(), 0);
		socket.sendDone(socket, client, data.slice(0, iResult));
	}

	ServerSocket::~ServerSocket() {
		ioMgr->close_socket(*this);
	}
}