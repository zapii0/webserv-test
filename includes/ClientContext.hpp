#ifndef CLIENTCONTEXT_HPP
#define CLIENTCONTEXT_HPP

#include <string>
#include <map>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <cerrno>
#include <sys/stat.h>
#include <fcntl.h>
#include "config/Parser.hpp"

# include <sys/socket.h> // socket() bind() listen() accept() recv() send()
# include <cstring>
# include <netinet/in.h> // sockaddr_in
# include <arpa/inet.h> // inet_ntoa() inet_addr() htons() htonl() ntohs() ntohl()
# include <poll.h>

enum ClientState {
    READING_HEADERS,
    READING_BODY,
    PROCESSING,
    WRITING_RESPONSE,
    FINISHED
};

class ClientContext {
    private:
        int         _client_fd;
        ClientState _state;
        size_t      _bytes_sent;
        pid_t       _cgi_pid;
        bool        _is_cgi;

    public:
        ServerConfig Config;
        std::string raw_buffer;
        std::string method;
        std::string path;
        std::string query_string;
        std::map<std::string, std::string> headers;
        std::string request_body;
        int status_code;
        std::string response_headers;
        std::string response_body;

        int cgi_pipe_in[2];
        int cgi_pipe_out[2];

        ClientContext(int fd) 
            : _client_fd(fd), 
              _state(READING_HEADERS), 
              _bytes_sent(0), 
              _cgi_pid(-1), 
              _is_cgi(false),
              Config(),
              status_code(200)
        {
            cgi_pipe_in[0] = -1; cgi_pipe_in[1] = -1;
            cgi_pipe_out[0] = -1; cgi_pipe_out[1] = -1;
        }

        ~ClientContext() {
            // (clearing pipes descriptors in a future)
        }

        int getClientFd() const { return _client_fd; }

        ClientState getState() const { return _state; }
        void setState(ClientState state) { _state = state; }

        size_t getBytesSent() const { return _bytes_sent; }
        void addBytesSent(size_t n) { _bytes_sent += n; }

        bool isCgi() const { return _is_cgi; }
        void setIsCgi(bool cgi) { _is_cgi = cgi; }

        pid_t getCgiPid() const { return _cgi_pid; }
        void setCgiPid(pid_t pid) { _cgi_pid = pid; }
};

#endif