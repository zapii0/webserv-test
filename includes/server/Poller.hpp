#ifndef POLLER_HPP
# define POLLER_HPP

# include <sys/socket.h> // socket() bind() listen() accept() recv() send()
# include <cstring>
# include <netinet/in.h> // sockaddr_in
# include <arpa/inet.h> // inet_ntoa() inet_addr() htons() htonl() ntohs() ntohl()
# include <poll.h>
# include "../ClientContext.hpp"
# include "ServerSocket.hpp"
# include <map>

class Poller
{
private:

	std::vector<struct pollfd> _pollfds;
	std::map<int, ClientContext> _clients;// pointer or not pointer
	std::vector<ServerSocket> _sockets;
	bool	_is_running;
	std::vector<ServerConfig> _servers_config;

	bool isListeningSocket(int fd) const;

	void handleListeningSocket(int fd);
    // void handleClientSocket(int fd);
	void	recvFromClient(int fd);
	void	sendToClient(int fd);

    void addPollFd(int fd, short events);
    void removePollFd(int fd);

	ServerConfig	findServerConfig(int fd);

public:
	Poller(std::vector<ServerConfig> servers);
	~Poller();

	void	runPollLoop();
};

#endif