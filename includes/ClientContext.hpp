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
#include <ctime>
#include <cstdio>
#include <sstream>
#include "config/Parser.hpp"

# include <sys/socket.h>
# include <cstring>
# include <netinet/in.h>
# include <arpa/inet.h>
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
        std::string resolved_path;
        std::string redirect_location;
        std::string query_string;
        std::map<std::string, std::string> headers;
        std::string request_body;
        time_t last_activity;

        int status_code;
        std::string response_headers;
        std::string response_body;
        std::string response_left;

        // Używamy tylko potoku do odczytu danych z CGI
        int cgi_pipe_out[2];
        std::string cgi_raw_output;
        time_t cgi_start_time;

        ClientContext(int fd) 
            : _client_fd(fd), 
              _state(READING_HEADERS), 
              _bytes_sent(0), 
              _cgi_pid(-1), 
              _is_cgi(false),
              Config(),
              last_activity(time(NULL)),
              status_code(200),
              cgi_raw_output(""),
              cgi_start_time(0)
        {
            cgi_pipe_out[0] = -1; 
            cgi_pipe_out[1] = -1;
        }

        ~ClientContext() {
            // Zamknięcie potoków wyjściowych
            if (cgi_pipe_out[0] != -1) close(cgi_pipe_out[0]);
            if (cgi_pipe_out[1] != -1) close(cgi_pipe_out[1]);

            // Automatyczne usunięcie pliku tymczasowego po zakończeniu żądania
            std::stringstream ss;
            ss << "/tmp/webserv_cgi_in_" << _client_fd;
            std::remove(ss.str().c_str());
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