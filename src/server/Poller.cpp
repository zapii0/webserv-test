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
        addPollFd(_sockets[i].getFd(), POLLIN);
    }

    for (size_t i = 0; i < _pollfds.size(); ++i)
    {
        std::cout << _pollfds[i].fd << _pollfds[i].events << _pollfds[i].revents << std::endl;
    }
}

Poller::~Poller() {}

// ******************************************************************************************* //
// HELPERS  *
// ******************************************************************************************* //

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
    int    port = -1;
    for (size_t i = 0; i < _sockets.size(); i++)
    {
        if (_sockets[i].getFd() == fd)
            port = _sockets[i].getPort();
    }
    for (size_t i = 0; i < _servers_config.size(); i++)
    {
        if (_servers_config[i].port == port)
            return _servers_config[i];
    }
    throw std::runtime_error("Config not found");
}

// ******************************************************************************************* //
// accept, receive, send  *
// ******************************************************************************************* //

void Poller::handleListeningSocket(int fd)
{
    std::cout << "\033[1;31m" << "server " << fd << " is ready to accept" << "\033[0m" << std::endl;
    int clientFd = accept(fd, NULL, NULL);
    
    if (clientFd < 0)
    {
        std::cerr << "Failed to accept connection." << std::endl;
        return;
    }

    ServerSocket::setNonBlocking(clientFd);

    _clients.insert(std::pair<int, ClientContext>(clientFd, ClientContext(clientFd)));
    _clients.at(clientFd).Config = findServerConfig(fd);
    std::cout << _clients.at(clientFd).Config.root << std::endl;
    addPollFd(clientFd, POLLIN);
}

void Poller::removePollFd(int fd)
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

void Poller::recvFromClient(int fd)
{
    std::cout << "\033[1;31m" << "receiving from client" << "\033[0m" << std::endl;
    
    char buffer[1024] = {0};

    _clients.at(fd).last_activity = time(NULL);
    int bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0)
    {
        std::cerr << "Failed to read from socket or client disconnected." << std::endl;
        if (_clients.find(fd) != _clients.end())
        {
            if (_clients.at(fd).isCgi())
            {
                if (_clients.at(fd).getCgiPid() > 0)
                {
                    kill(_clients.at(fd).getCgiPid(), SIGKILL);
                    waitpid(_clients.at(fd).getCgiPid(), NULL, WNOHANG);
                }
                int pipe_fd = _clients.at(fd).cgi_pipe_out[0];
                if (pipe_fd != -1)
                {
                    close(pipe_fd);
                    removePollFd(pipe_fd);
                    _pipe_to_client_fd.erase(pipe_fd);
                }
            }
            _clients.erase(fd);
        }
        close(fd);
        removePollFd(fd);
        return ;
    }

    _clients.at(fd).raw_buffer += std::string(buffer, bytes_read);

    RequestParser CheckRequest(_clients.at(fd).raw_buffer);
    Result result = CheckRequest.parseRequest(_clients.at(fd));
    // std::cout << "Raw request: " << _clients.at(fd).raw_buffer << std::endl;
    if (result == PARSE_COMPLETE)
    {
        std::cout << "\033[1;31m" << "request parsed successfully" << "\033[0m" << std::endl;
        std::cout << "Raw request: " << _clients.at(fd).raw_buffer;
        ExecMethods(_clients.at(fd));
        if (_clients.at(fd).isCgi())
        {
            int pipe_fd = _clients.at(fd).cgi_pipe_out[0];
            if (pipe_fd != -1)
            {
                _pipe_to_client_fd[pipe_fd] = fd;
                addPollFd(pipe_fd, POLLIN);
            }
        }
        else
        {
            for (size_t i = 0; i < _pollfds.size(); ++i)
            {
                if (_pollfds[i].fd == fd)
                {
                    _pollfds[i].events = POLLOUT;
                    return ;
                }
            }
        }
    }
    else if (result == PARSE_ERROR)
    {
        std::cout << "Raw request: " << _clients.at(fd).raw_buffer;
        errorPageGetter(_clients.at(fd));
        BuildHeaders(_clients.at(fd), _clients.at(fd).path);
        for (size_t i = 0; i < _pollfds.size(); ++i)
        {
            if (_pollfds[i].fd == fd)
            {
                _pollfds[i].events = POLLOUT;
                return ;
            }
        }
    }
}

void Poller::handleCgiPipe(int pipe_fd)
{
    if (_pipe_to_client_fd.find(pipe_fd) == _pipe_to_client_fd.end())
        return;
    int client_fd = _pipe_to_client_fd[pipe_fd];
    if (_clients.find(client_fd) == _clients.end())
    {
        close(pipe_fd);
        removePollFd(pipe_fd);
        _pipe_to_client_fd.erase(pipe_fd);
        return;
    }

    ClientContext &ctx = _clients.at(client_fd);
    ctx.last_activity = time(NULL);
    char buffer[4096];
    ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        ctx.cgi_raw_output.append(buffer, bytes_read);
    }
    else if (bytes_read <= 0)
    {
        close(pipe_fd);
        removePollFd(pipe_fd);
        _pipe_to_client_fd.erase(pipe_fd);
        ctx.cgi_pipe_out[0] = -1;

        int status = 0;
        if (ctx.getCgiPid() > 0)
        {
            waitpid(ctx.getCgiPid(), &status, WNOHANG);
            ctx.setCgiPid(-1);
        }
        ctx.setIsCgi(false);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0 && ctx.cgi_raw_output.empty())
        {
            ctx.status_code = 502;
            errorPageGetter(ctx);
            BuildHeaders(ctx, ctx.path);
        }
        else
        {
            parseCgiOutput(ctx);
        }

        for (size_t i = 0; i < _pollfds.size(); ++i)
        {
            if (_pollfds[i].fd == client_fd)
            {
                _pollfds[i].events = POLLOUT;
                break;
            }
        }
    }
}

void Poller::checkCgiTimeouts()
{
    time_t now = time(NULL);
    for (std::map<int, ClientContext>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        ClientContext &ctx = it->second;

        if (!ctx.isCgi() && now - ctx.last_activity >= 5)
        {
            close(it->first);
            removePollFd(it->first);
            _clients.erase(it->first);
            return;
        }

        if (ctx.isCgi() && ctx.cgi_start_time > 0 && (now - ctx.cgi_start_time) >= 5)
        {
            if (ctx.getCgiPid() > 0)
            {
                kill(ctx.getCgiPid(), SIGKILL);
                waitpid(ctx.getCgiPid(), NULL, WNOHANG);
                ctx.setCgiPid(-1);
            }
            int pipe_fd = ctx.cgi_pipe_out[0];
            if (pipe_fd != -1)
            {
                close(pipe_fd);
                removePollFd(pipe_fd);
                _pipe_to_client_fd.erase(pipe_fd);
                ctx.cgi_pipe_out[0] = -1;
            }
            ctx.setIsCgi(false);
            ctx.status_code = 504;
            errorPageGetter(ctx);
            BuildHeaders(ctx, ctx.path);

            int client_fd = ctx.getClientFd();
            for (size_t i = 0; i < _pollfds.size(); ++i)
            {
                if (_pollfds[i].fd == client_fd)
                {
                    _pollfds[i].events = POLLOUT;
                    break;
                }
            }
        }
    }
}

void Poller::sendToClient(int fd)
{
    std::cout << "\033[1;31m" << "sending to client" << "\033[0m" << std::endl;
    _clients.at(fd).last_activity = time(NULL);

    std::string response = _clients.at(fd).response_headers + _clients.at(fd).response_body;
    size_t offset = _clients.at(fd).getBytesSent();
    size_t remaining = response.size() - offset;

    std::cout << "Response:\n" << response << std::endl;
    if (remaining == 0)
    {
        if (_clients.at(fd).isCgi())
        {
            if (_clients.at(fd).getCgiPid() > 0)
            {
                kill(_clients.at(fd).getCgiPid(), SIGKILL);
                waitpid(_clients.at(fd).getCgiPid(), NULL, WNOHANG);
            }
            int pipe_fd = _clients.at(fd).cgi_pipe_out[0];
            if (pipe_fd != -1)
            {
                close(pipe_fd);
                removePollFd(pipe_fd);
                _pipe_to_client_fd.erase(pipe_fd);
            }
        }
        close(fd);
        removePollFd(fd);
        _clients.erase(fd);
        return ;
    }

    ssize_t sent = send(fd, response.c_str() + offset, remaining, 0);
    if (sent > 0)
        _clients.at(fd).addBytesSent(static_cast<size_t>(sent));
    else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return ;
    else
    {
        std::cerr << "send failed: " << std::strerror(errno) << std::endl;
        if (_clients.at(fd).isCgi())
        {
            if (_clients.at(fd).getCgiPid() > 0)
            {
                kill(_clients.at(fd).getCgiPid(), SIGKILL);
                waitpid(_clients.at(fd).getCgiPid(), NULL, WNOHANG);
            }
            int pipe_fd = _clients.at(fd).cgi_pipe_out[0];
            if (pipe_fd != -1)
            {
                close(pipe_fd);
                removePollFd(pipe_fd);
                _pipe_to_client_fd.erase(pipe_fd);
            }
        }
        close(fd);
        removePollFd(fd);
        _clients.erase(fd);
        return ;
    }

    if (response.size() == _clients.at(fd).getBytesSent())
    {
        if (_clients.at(fd).isCgi())
        {
            if (_clients.at(fd).getCgiPid() > 0)
            {
                kill(_clients.at(fd).getCgiPid(), SIGKILL);
                waitpid(_clients.at(fd).getCgiPid(), NULL, WNOHANG);
            }
            int pipe_fd = _clients.at(fd).cgi_pipe_out[0];
            if (pipe_fd != -1)
            {
                close(pipe_fd);
                removePollFd(pipe_fd);
                _pipe_to_client_fd.erase(pipe_fd);
            }
        }
        close(fd);
        removePollFd(fd);
        _clients.erase(fd);
    }
    else
        std::cout << "\nstill sending...\n";
}

// ******************************************************************************************* //
// MAIN LOOP  *
// ******************************************************************************************* //

void Poller::runPollLoop()
{
    _is_running = true;
    while (_is_running)
    {
        checkCgiTimeouts();

        int result = poll(&_pollfds[0], _pollfds.size(), 1000);
        if (result < 0)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll error");
        }
        for (size_t i = 0; i < _pollfds.size(); ++i)
        {
            if (_pollfds[i].revents == 0)
                continue;
            int fd = _pollfds[i].fd;

            if (!isListeningSocket(fd) &&
                _pipe_to_client_fd.find(fd) == _pipe_to_client_fd.end() &&
                _clients.find(fd) == _clients.end())
                continue;

            if (_pipe_to_client_fd.find(fd) != _pipe_to_client_fd.end())
            {
                if (_pollfds[i].revents & (POLLIN | POLLHUP | POLLERR))
                    handleCgiPipe(fd);
            }
            else if (isListeningSocket(fd))
            {
                if (_pollfds[i].revents & POLLIN)
                     handleListeningSocket(fd);
            }
            else if (_pollfds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                recvFromClient(fd);
            }
            else if (_pollfds[i].revents & POLLOUT)
            {
                sendToClient(fd);
            }
        }
    }
}