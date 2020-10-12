#pragma once

#include "Sockets.h"

namespace Network {
	class ClientSocket{
	protected:
		uintptr_t handle;

		enum class SocketState {
			Open,
			Closed,
			Error
		};
		SocketState socketState;

	public:
		ClientSocket() {}
		virtual ~ClientSocket() {}

		// Returns number of bytes read or throws
		virtual uint32_t send_data(const void* data, uint32_t data_length) = 0;

		// Returns nullopt if no data is read (no data in buffer or error), otherwise returns number of bytes read
		// Check socketState to determine if there was an error
		virtual uint32_t receive_data(void* data, uint32_t data_length) = 0;


		virtual bool is_reliable() = 0;

		uintptr_t get_handle() { return handle; }
		SocketState get_socket_state() { return socketState; }

		ClientSocket(const ClientSocket& other) = delete;
		ClientSocket& operator=(const ClientSocket& other) = delete;
		ClientSocket(ClientSocket&& other) = delete;
		ClientSocket& operator=(ClientSocket&& other) = delete;
	};

	

	class TCPClientSocket : public ClientSocket {
	public:
		TCPClientSocket(const char* server, uint16_t serverPort, bool blocking);
		virtual ~TCPClientSocket() override;

		bool is_reliable() override { return true; }

		virtual uint32_t send_data(const void* data, uint32_t data_length) override;
		virtual uint32_t receive_data(void* data, uint32_t data_length) override;
	};

	class UDPClientSocket : public ClientSocket {
	public:
		UDPClientSocket(const char* server, uint16_t serverPort, bool blocking);
		virtual ~UDPClientSocket() override;

		bool is_reliable() override { return false; }

		virtual uint32_t send_data(const void* data, uint32_t data_length) override;
		virtual uint32_t receive_data(void* data, uint32_t data_length) override;
	};
}