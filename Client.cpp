
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "Sockets.h"
#include "ClientSocket.h"
#include <stdexcept>
#include <string>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;

namespace Network {

static WSADATA wsaData;

void global_init() {
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		throw runtime_error("WSAStartup error");
	}
}
void global_deinit() {
	WSACleanup();
}

TCPClientSocket::TCPClientSocket(const char* server, uint16_t serverPort, bool blocking) {
	SOCKET new_socket = INVALID_SOCKET;
	addrinfo* result = NULL,
		hints;

	int iResult;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	char portString[20];
	sprintf(portString, "%d", serverPort);
	iResult = getaddrinfo(server, portString, &hints, &result);
	if (iResult != 0) {
		throw runtime_error(string("getaddrinfo error: ") + to_string(iResult));
	}

	// Attempt to connect to an address until one succeeds
	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		new_socket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (new_socket == INVALID_SOCKET) {
			freeaddrinfo(result);
			throw runtime_error("Socket creation error");
		}

		// Connect to server.
		iResult = connect(new_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(new_socket);
			new_socket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (new_socket == INVALID_SOCKET) {
		throw runtime_error("Unable to establish connection to server");
	}


	if (!blocking) {
		unsigned long x;
		ioctlsocket(new_socket, FIONBIO, &x);
	}

	handle = new_socket;
	socketState = SocketState::Open;
}

TCPClientSocket::~TCPClientSocket() {
	closesocket(handle);
}

uint32_t TCPClientSocket::send_data(const void* data, uint32_t data_length) {
	// Send an initial buffer
	int iResult = send(handle, static_cast<const char *>(data), data_length, 0);
	if (iResult <= 0) {
		socketState = SocketState::Error;
		throw runtime_error(string("TCP send error: " + to_string(iResult)));
	}

	return iResult;
}
uint32_t TCPClientSocket::receive_data(void* data, uint32_t data_length) {
	int iResult = recv(handle, static_cast<char *>(data), data_length, 0);
	if (iResult < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			return 0;
		}
		socketState = SocketState::Error;
		throw runtime_error(string("TCP receive error: " + to_string(iResult)));
	}
	return iResult;
}

}