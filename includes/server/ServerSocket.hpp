#ifndef SERVER_SOCKET_HPP
# define SERVER_SOCKET_HPP

# include <sys/socket.h> // socket() bind() listen() accept() recv() send()
# include <cstring>
# include <netinet/in.h> // sockaddr_in
# include <arpa/inet.h> // inet_ntoa() inet_addr() htons() htonl() ntohs() ntohl()
# include <poll.h>
# include "../ClientContext.hpp"

class ServerSocket
{
private:
	int			_fd;
	int			_port;
	sockaddr_in	_address;
public:
	ServerSocket(int port);
	~ServerSocket();

    void bind_n_listenSocket();

    int getFd() const { return _fd; }
	int	getPort() const { return _port; }
};

#endif