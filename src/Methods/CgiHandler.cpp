#include "../../includes/Methods.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <vector>

static std::string absolutePath(const std::string &path) {
    if (path.empty() || path[0] == '/')
        return path;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return path;
    if (path.compare(0, 2, "./") == 0)
        return std::string(cwd) + path.substr(1);
    return std::string(cwd) + "/" + path;
}

bool isCgiRequest(ClientContext &ctx, std::string &script_path, std::string &interpreter) {
    const LocationConfig *matched_loc = NULL;
    for (size_t i = 0; i < ctx.Config.locations.size(); ++i) {
        if (!ctx.Config.locations[i].cgi_ext.empty() &&
            ctx.path.length() >= ctx.Config.locations[i].cgi_ext.length() &&
            ctx.path.substr(ctx.path.length() - ctx.Config.locations[i].cgi_ext.length()) ==
                ctx.Config.locations[i].cgi_ext) {
            matched_loc = &ctx.Config.locations[i];
            break;
        }
    }

    std::string ext = "";
    if (matched_loc != NULL) {
        ext = matched_loc->cgi_ext;
    } else if (ctx.path.length() >= 3 && ctx.path.substr(ctx.path.length() - 3) == ".py") {
        ext = ".py";
    }

    if (ext.empty())
        return false;

    if (ctx.path.length() < ext.length() || ctx.path.substr(ctx.path.length() - ext.length()) != ext) {
        if (ctx.path.find(ext) == std::string::npos)
            return false;
    }

    std::string full_path = ctx.resolved_path.empty() ?
        RootPathJoin(ctx.path, ctx.Config.root) : ctx.resolved_path;
    if (access(full_path.c_str(), F_OK) != 0) {
        std::string alt_path = "." + ctx.path;
        if (access(alt_path.c_str(), F_OK) == 0)
            full_path = alt_path;
    }

    script_path = absolutePath(full_path);

    if (matched_loc != NULL && !matched_loc->cgi_path.empty() &&
        access(matched_loc->cgi_path.c_str(), X_OK) == 0) {
        interpreter = matched_loc->cgi_path;
    } else if (access("./cgi_test", X_OK) == 0) {
        interpreter = absolutePath("./cgi_test");
    } else if (access("./cgi_tester", X_OK) == 0) {
        interpreter = absolutePath("./cgi_tester");
    } else {
        interpreter = "/usr/bin/python3";
    }

    return true;
}

void ExecCgi(ClientContext &ctx, const std::string &script_path, const std::string &interpreter) {
    struct stat st;
    if (stat(script_path.c_str(), &st) != 0) {
        ctx.status_code = 404;
        errorPageGetter(ctx);
        BuildHeaders(ctx, script_path);
        return;
    }

    // 1. Zapis ciała zapytania do pliku dyskowego (zwolnione z reguł poll())
    std::stringstream tmp_name_ss;
    tmp_name_ss << "/tmp/webserv_cgi_in_" << ctx.getClientFd();
    std::string tmp_in_file = tmp_name_ss.str();

    if (!ctx.request_body.empty()) {
        int tmp_fd = open(tmp_in_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (tmp_fd >= 0) {
            write(tmp_fd, ctx.request_body.c_str(), ctx.request_body.size());
            close(tmp_fd);
        }
    }

    // 2. Pipe używamy WYŁĄCZNIE do odczytywania wyniku skryptu
    if (pipe(ctx.cgi_pipe_out) < 0) {
        ctx.status_code = 500;
        errorPageGetter(ctx);
        BuildHeaders(ctx, script_path);
        return;
    }

    fcntl(ctx.cgi_pipe_out[0], F_SETFL, O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        close(ctx.cgi_pipe_out[0]); close(ctx.cgi_pipe_out[1]);
        ctx.cgi_pipe_out[0] = ctx.cgi_pipe_out[1] = -1;
        ctx.status_code = 500;
        errorPageGetter(ctx);
        BuildHeaders(ctx, script_path);
        return;
    }

    if (pid == 0) { // Child process
        size_t last_slash = script_path.find_last_of("/");
        if (last_slash != std::string::npos) {
            std::string dir = script_path.substr(0, last_slash);
            chdir(dir.c_str());
        }

        // 3. Skrypt czyta ze zrzuconego pliku, zamiast z zablokowanego pipe'a
        if (!ctx.request_body.empty()) {
            int tmp_fd = open(tmp_in_file.c_str(), O_RDONLY);
            if (tmp_fd >= 0) {
                dup2(tmp_fd, STDIN_FILENO);
                close(tmp_fd);
            }
        } else {
            int null_fd = open("/dev/null", O_RDONLY);
            if (null_fd >= 0) {
                dup2(null_fd, STDIN_FILENO);
                close(null_fd);
            }
        }

        dup2(ctx.cgi_pipe_out[1], STDOUT_FILENO);
        close(ctx.cgi_pipe_out[0]);
        close(ctx.cgi_pipe_out[1]);
        if (ctx.getClientFd() != -1)
            close(ctx.getClientFd());

        std::vector<std::string> env_strings;
        env_strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        env_strings.push_back("SERVER_PROTOCOL=HTTP/1.1");
        env_strings.push_back("SERVER_SOFTWARE=webserv/1.0");
        env_strings.push_back("REQUEST_METHOD=" + ctx.method);
        env_strings.push_back("QUERY_STRING=" + ctx.query_string);
        env_strings.push_back("SCRIPT_NAME=" + ctx.path);
        env_strings.push_back("SCRIPT_FILENAME=" + script_path);
        env_strings.push_back("PATH_INFO=" + ctx.path);
        env_strings.push_back("REQUEST_URI=" + ctx.path);
        env_strings.push_back("SERVER_NAME=" + ctx.Config.server_name);
        std::stringstream ss_port;
        ss_port << ctx.Config.port;
        env_strings.push_back("SERVER_PORT=" + ss_port.str());
        env_strings.push_back("REDIRECT_STATUS=200");

        std::stringstream ss_len;
        ss_len << ctx.request_body.size();
        env_strings.push_back("CONTENT_LENGTH=" + ss_len.str());

        std::map<std::string, std::string>::const_iterator ct_it = ctx.headers.find("Content-Type");
        if (ct_it == ctx.headers.end()) ct_it = ctx.headers.find("content-type");
        if (ct_it != ctx.headers.end()) {
            env_strings.push_back("CONTENT_TYPE=" + ct_it->second);
        }

        for (std::map<std::string, std::string>::const_iterator it = ctx.headers.begin(); it != ctx.headers.end(); ++it) {
            std::string key = "HTTP_";
            for (size_t i = 0; i < it->first.length(); ++i) {
                char c = it->first[i];
                if (c == '-') key += '_';
                else key += toupper(c);
            }
            env_strings.push_back(key + "=" + it->second);
        }

        char **envp = new char*[env_strings.size() + 1];
        for (size_t i = 0; i < env_strings.size(); ++i) {
            envp[i] = const_cast<char*>(env_strings[i].c_str());
        }
        envp[env_strings.size()] = NULL;

        char *argv[3];
        argv[0] = const_cast<char*>(interpreter.c_str());
        argv[1] = const_cast<char*>(script_path.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, envp);
        delete[] envp;
        exit(1);
    }

    // Parent process
    close(ctx.cgi_pipe_out[1]);
    ctx.cgi_pipe_out[1] = -1;

    ctx.setCgiPid(pid);
    ctx.setIsCgi(true);
    ctx.cgi_start_time = time(NULL);
    ctx.cgi_raw_output.clear();
}

void parseCgiOutput(ClientContext &ctx) {
    std::string raw = ctx.cgi_raw_output;
    std::size_t sep_pos = raw.find("\r\n\r\n");
    std::size_t sep_len = 4;
    if (sep_pos == std::string::npos) {
        sep_pos = raw.find("\n\n");
        sep_len = 2;
    }

    if (sep_pos == std::string::npos) {
        ctx.status_code = 200;
        ctx.response_body = raw;
    } else {
        std::string headers_str = raw.substr(0, sep_pos);
        ctx.response_body = raw.substr(sep_pos + sep_len);

        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < headers_str.length()) {
            std::size_t end = headers_str.find("\n", start);
            if (end == std::string::npos)
                end = headers_str.length();
            std::string line = headers_str.substr(start, end - start);
            if (!line.empty() && line[line.length() - 1] == '\r')
                line.erase(line.length() - 1);
            lines.push_back(line);
            start = end + 1;
        }

        ctx.status_code = 200;
        std::string cgi_content_type = "";

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = lines[i];
            if (line.empty()) continue;
            std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                while (!value.empty() && value[0] == ' ') value.erase(0, 1);

                if (key == "Status" || key == "status") {
                    ctx.status_code = std::atoi(value.c_str());
                    if (ctx.status_code == 0) ctx.status_code = 200;
                } else if (key == "Content-Type" || key == "content-type") {
                    cgi_content_type = value;
                }
            }
        }

        std::stringstream len_ss, status_ss;
        len_ss << ctx.response_body.size();
        status_ss << ctx.status_code;

        std::string status_msg = "OK";
        if (ctx.status_code == 200) status_msg = "OK";
        else if (ctx.status_code == 400) status_msg = "Bad Request";
        else if (ctx.status_code == 403) status_msg = "Forbidden";
        else if (ctx.status_code == 404) status_msg = "Not Found";
        else if (ctx.status_code == 500) status_msg = "Internal Server Error";
        else status_msg = "OK";

        if (cgi_content_type.empty())
            cgi_content_type = "text/html";

        ctx.response_headers = "HTTP/1.1 " + status_ss.str() + " " + status_msg + "\r\n";
        ctx.response_headers += "Content-Type: " + cgi_content_type + "\r\n";
        ctx.response_headers += "Content-Length: " + len_ss.str() + "\r\n";
        ctx.response_headers += "\r\n";
        return;
    }

    BuildHeaders(ctx, ctx.path);
}