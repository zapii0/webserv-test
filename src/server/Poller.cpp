#include "../../includes/server/Poller.hpp"
#include "../../includes/http/RequestParser.hpp"
#include "../../includes/Methods.hpp"

Poller::Poller(std::vector<ServerConfig> servers)
{
	_is_running = false;
	_servers_config = servers;
	for (size_t i = 0; i < servers.size(); ++i)
	{
		_sockets.push_back(servers[i].port);
		_sockets[i].bind_n_listenSocket();
		// std::cout << "Socket " << (i + 1) << ":	fd:	" << _sockets[i].getFd() << "	port:	"<< _sockets[i].getPort() << std::endl; // debug
		addPollFd(_sockets[i].getFd(), POLLIN);
	}

	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		std::cout << _pollfds[i].fd <<_pollfds[i].events << _pollfds[i].revents << std::endl;
	}
}

Poller::~Poller() {}

bool Poller::isListeningSocket(int fd) const
{
	for (size_t i = 0; i < _sockets.size(); i++)
	{
		if (fd == _sockets[i].getFd())
			return (true);
	}
	return (false);
}

void Poller::addPollFd(int fd, short events)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;

	_pollfds.push_back(pfd);
}

ServerConfig Poller::findServerConfig(int fd)
{
	int	port;
	for (size_t i = 0; i < _sockets.size(); i++)
	{
		if (_sockets[i].getFd() == fd)
		{
			port = _sockets[i].getPort();
		}
	}
	for (size_t i = 0; i < _servers_config.size(); i++)
	{
		if (_servers_config[i].port == port)
		{
			return _servers_config[i];
		}
	}
	throw std::runtime_error("Config not found");
}

void	Poller::handleListeningSocket(int fd)
{
	std::cout << "\033[1;31m" << "server " << fd << " is ready to accept" << "\033[0m" << std::endl;
	int clientFd = accept(fd, NULL, NULL);
	if (clientFd < 0)
	{
		std::cerr << "Failed to accept connection." << std::endl;
		return;
	}

    _clients.insert(std::pair<int, ClientContext>(clientFd, ClientContext(clientFd)));
	_clients.at(clientFd).Config = findServerConfig(fd);
	std::cout << _clients.at(clientFd).Config.root << std::endl;
	addPollFd(clientFd, POLLIN);
}

void	Poller::removePollFd(int fd)
{
	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == fd)
		{
			_pollfds.erase(_pollfds.begin() + i);
			return ;
		}
	}
}

void	Poller::recvFromClient(int fd)
{
	std::cout << "\033[1;31m" << "receiving from client" << "\033[0m" << std::endl;
	
	char buffer[1024] = {0};

	int bytes_read = recv(fd, buffer, sizeof(buffer), 0); // The recv() function is used to receive messages from a socket. It is typically used with connection-oriented sockets (e.g., TCP) to receive data from a connected peer.
	
	if (bytes_read < 0)
	{
		std::cerr << "Failed to read from socket." << std::endl;
		close(fd);
		return ;
	}

	std::cout << "Received: " << buffer << std::endl; // debug

	_clients.at(fd).raw_buffer += buffer;

	// check if the recv is done
	RequestParser	CheckRequest(_clients.at(fd).raw_buffer);
	Result	result = CheckRequest.parseRequest(_clients.at(fd));
	switch (result) // debug
	{
	case PARSE_INCOMPLETE:
		std::cout << "INCOMPLETE" << std::endl;
		break;
	case PARSE_COMPLETE:
		std::cout << "COMPLETE" << std::endl;
		break;
	case PARSE_ERROR:
		std::cout << "ERROR" << std::endl;
		break;
	default:
		break;
	}
	if (result == PARSE_COMPLETE)
	{
		// Headers are complete.
		// _clients[fd].setState(WRITING_RESPONSE);
		// create a response
		ExecMethods(_clients.at(fd));
		for (size_t i = 0; i < _pollfds.size(); ++i)
		{
			if (_pollfds[i].fd == fd)
			{
				_pollfds[i].events = POLLOUT; // or POLLIN | POLLOUT depending on the connection policy - read http1.0 rfcs
				return ;
			}
		}
	}
}

void	Poller::sendToClient(int fd)
{
	std::cout << "\033[1;31m" << "sending to client" << "\033[0m" << std::endl;
	
	// std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 65\r\n\r\nHello I'm under the water, please help me! It's too much raining!";
	std::string	response = _clients.at(fd).response_headers + _clients.at(fd).response_body;
	_clients.at(fd).addBytesSent(send(fd, response.c_str(), strlen(response.c_str()), 0));
	
	// check if all response is sent
	// std::cout << response.size() << " " << _clients.at(fd).getBytesSent() << std::endl; //debug

	if (response.size() == _clients.at(fd).getBytesSent())
	{
		// std::cout << "\nall sent\n";
		close(fd); // depends if its keep-alive or not
		removePollFd(fd);
		_clients.erase(fd);
	}
	else
		std::cout << "\nstill sending...\n";
}

void	Poller::runPollLoop()
{
	_is_running = true;
	while (_is_running)
	{
		int result = poll(&_pollfds[0], _pollfds.size(), -1); // -1 means infinite blocking - no timeout
		if (result < 0)
        {
            // handle error
			throw std::runtime_error("poll error"); // idk if exception here is ok
            continue;
        }
		for (size_t i = 0; i < _pollfds.size(); ++i)
        {
            if (_pollfds[i].revents == 0)
                continue;
			else if ((_pollfds[i].revents & POLLIN) || (_pollfds[i].revents & POLLOUT)) //bitmasks
			{
				int fd = _pollfds[i].fd;

				if (isListeningSocket(fd))
					handleListeningSocket(fd);
				else if (_pollfds[i].revents & POLLIN)
					recvFromClient(fd);
				else
					sendToClient(fd);
			}
        }
	}
}
