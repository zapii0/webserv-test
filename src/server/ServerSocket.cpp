#include "../../includes/server/ServerSocket.hpp"

ServerSocket::ServerSocket(int port) : _port(port)
{
	_fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET - IPv4 Internet protocols, SOCK_STREAM - TCP, 0 - default protocol (TCP for SOCK_STREAM)

	if (_fd < 0)
	{
		throw std::runtime_error("Failed to accept connection.");
		return;
	}

	_address.sin_family = AF_INET; // AF_INET - IPv4 Internet protocols
	_address.sin_addr.s_addr = INADDR_ANY; // The INADDR_ANY constant allows the server to accept connections on any of the host's IP addresses.
	_address.sin_port = htons(port); // The htons() function converts the unsigned short integer hostshort from host byte order to network byte order.

	setNonBlocking(_fd);
}

ServerSocket::~ServerSocket() {}

void	ServerSocket::bind_n_listenSocket()
{
	int opt = 1;

	if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) // for recompilation
	{
		close(_fd);
		throw std::runtime_error("setsockopt failed");
	}
	
	if (bind(_fd, (struct sockaddr*)&_address, sizeof(_address)) < 0)
	{
		close(_fd);
		std::cerr << "bind failed: " << std::strerror(errno) << std::endl;
		throw std::runtime_error("Failed to bind socket.");
	}
	
	if (listen(_fd, SOMAXCONN) < 0) // marks the socket as a passive socket, that is, as a socket that will be used to accept incoming connection requests using accept(). SOMAXCONN is a constant that defines the maximum length for the queue of pending connections it's set to the maximum value allowed by the system (you can check in /proc/sys/net/core/somaxconn)
	{
		close(_fd);
		throw std::runtime_error("Failed to listen on socket.");
	}
}

void ServerSocket::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        throw std::runtime_error("fcntl F_GETFL failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL failed");
}
