#pragma once

#include "Sockets.h"
#include "Slice.h"
#include <list>
#include <thread>
#include <optional>

namespace Network {
	class ServerIOManager;

	class ServerSocket {
		friend class ServerIOManager;
		friend class ServerIOManagerPoll;

	public:
		struct Client {
			friend class ServerIOManager;
			friend class ServerIOManagerPoll;

			friend class TCPServerSocket;
			//friend class UDPServerSocket;
		private:
			uintptr_t handle;

		public:
			IP ip;
			uint16_t clientPort;
		};

		// Return a client object if the connection should be made, null to close the connection (e.g. for IP bans)
		using NewConnection = std::function <std::unique_ptr<Client>(ServerSocket&, IP ip, uint16_t clientPort)>;

		using DataRecieved = std::function<void(ServerSocket&, Client&, Slice<const uint8_t> data)>;

		// data is the pointer&size that was passed to sendData()
		using SendComplete = std::function<void(ServerSocket&, Client&, Slice<const uint8_t> data)>;

		using ConnectionLost = std::function<void(ServerSocket&, Client&)>;


	protected:
		ServerIOManager* ioMgr; // Pointer guaranteed valid for lifetime of this object (can be set to nullptr)

		uint16_t port;
		
		// 2 sockets are created - one for ipv4, one for ipv6
		uintptr_t ipv4Handle;
		std::optional<uintptr_t> ipv6Handle;

		std::list<std::unique_ptr<Client>> clients;

		

		DataRecieved dataRecvd;
		SendComplete sendDone;

		ServerSocket(ServerIOManager* ioMgr_, uint16_t port_, uintptr_t ipv4Handle_, std::optional<uintptr_t> ipv6Handle_, DataRecieved dataRecvd_, SendComplete sendDone_) :
			ioMgr(ioMgr_), port(port_), ipv4Handle(ipv4Handle_), ipv6Handle(ipv6Handle_), dataRecvd(dataRecvd_), sendDone(sendDone_) {}
	public:

		uintptr_t get_ipv4_handle() { return ipv4Handle; }
		std::optional<uintptr_t> get_ipv6_handle() { return ipv6Handle; }

		virtual bool is_reliable()=0;

		//ServerSocket(uintptr_t ipv4Handle, std::optional<uintptr_t> ipv6Handle) {}

		ServerSocket(const ServerSocket& other) = delete;
		ServerSocket& operator=(const ServerSocket& other) = delete;
		ServerSocket(ServerSocket&& other) = delete;
		ServerSocket& operator=(ServerSocket&& other) = delete;

		~ServerSocket();

	};

	class TCPServerSocket : public ServerSocket {
		friend class ServerIOManager;
		friend class ServerIOManagerPoll;
	private:
		NewConnection newConn;
		ConnectionLost connLost;

	public:
		TCPServerSocket(ServerIOManager* ioMgr, uint16_t port_, uintptr_t ipv4Handle_, std::optional<uintptr_t> ipv6Handle_, NewConnection newConn_, ConnectionLost connLost_,
			DataRecieved dataRecvd_, SendComplete sendDone_) :
			ServerSocket(ioMgr, port_, ipv4Handle_, ipv6Handle_, dataRecvd_, sendDone_), newConn(newConn_), connLost(connLost_) {}
	
		bool is_reliable() override { return true; }
	};



	// Create one IO Manager per thread
	class ServerIOManager {
		friend class ServerSocket;
	private:
		virtual void close_socket(ServerSocket&)=0;

	public:

		virtual std::shared_ptr<ServerSocket> add_tcp_socket(uint16_t port, ServerSocket::NewConnection newConn, ServerSocket::ConnectionLost connLost,
			ServerSocket::DataRecieved dataRecvd, ServerSocket::SendComplete sendDone) = 0;
		//virtual std::shared_ptr<ServerSocket> add_udp_socket(uint16_t port, ServerSocket::DataRecieved dataRecvd, ServerSocket::SendComplete sendDone) = 0;


		// Blocks the thread. Sockets will call their callbacks
		// Might return early.
		// timeout is in milliseconds
		virtual void listen(uint32_t timeout) = 0;

		// Blocks indefinitely
		virtual void listen() = 0;


		// ServerSocket must be owned by this IO Manager
		virtual void send(ServerSocket&, ServerSocket::Client&, Slice<const uint8_t> data) = 0;
	};

	// Tries a non-blocking read on every socket in listen().
	class ServerIOManagerPoll : public ServerIOManager {
	private:
		std::list<std::shared_ptr<ServerSocket>> sockets;


		void poll_tcp_socket(TCPServerSocket& socket, uintptr_t handle);
		void poll_tcp_socket(TCPServerSocket& socket);

		virtual void close_socket(ServerSocket&) override;
	public:
		ServerIOManagerPoll();

		virtual std::shared_ptr<ServerSocket> add_tcp_socket(uint16_t port, ServerSocket::NewConnection newConn, ServerSocket::ConnectionLost connLost,
			ServerSocket::DataRecieved dataRecvd, ServerSocket::SendComplete sendDone) override;
		//virtual std::shared_ptr<ServerSocket> add_udp_socket(uint16_t port, ServerSocket::DataRecieved dataRecvd, ServerSocket::SendComplete sendDone) override;

		// Does not block, always returns immediately after polling all sockets.
		virtual void listen(uint32_t timeout) override;


		virtual void listen() override;

		virtual void send(ServerSocket&, ServerSocket::Client&, Slice<const uint8_t> data) override;
	};

#ifdef linux
	/*class ServerIOManagerEPoll : public ServerIOManager {

	};

	class ServerIOManagerIOUring : public ServerIOManager {

	};*/
#endif

}