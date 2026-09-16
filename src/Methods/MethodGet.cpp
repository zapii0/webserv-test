#include "../../includes/ClientContext.hpp"
#include "../../includes/Methods.hpp"
#include <sstream>

void    GetMethod(ClientContext& ctx) {
    std::string full_path = RootPathJoin(ctx.path, ctx.Config.root);
    if (isPathSafe(full_path))
        FileCheck(full_path, ctx);
    else
        ctx.status_code = 400;

    if (ctx.status_code == 200)
        OpenAndReadFile(full_path, ctx);
    else
        errorPageGetter(ctx);

    BuildHeaders(ctx, full_path);

    std::cout << "Full Path: " << full_path << std::endl;
}

// Building headers and error headers
void    BuildHeaders(ClientContext& ctx, std::string full_path) {
    std::stringstream len_ss;
    std::stringstream status_ss;
    std::string status_msg;
    std::string mimeType = "text/html";

    len_ss << ctx.response_body.size();
    status_ss << ctx.status_code;

    if (ctx.status_code == 200)
    {
        status_msg = "OK";
        mimeType = MimeTypeSetter(full_path);
    }
    else if (ctx.status_code == 201)
        status_msg = "Created";
    else if (ctx.status_code == 204)
        status_msg = "No Content";
    else if (ctx.status_code == 301)
        status_msg = "Moved Permanently";
    else if (ctx.status_code == 302)
        status_msg = "Found";
    else if (ctx.status_code == 400)
        status_msg = "Bad Request";
    else if (ctx.status_code == 403)
        status_msg = "Forbidden";
    else if (ctx.status_code == 404)
        status_msg = "Not Found";
    else if (ctx.status_code == 405)
        status_msg = "Method Not Allowed";
    else if (ctx.status_code == 409)
        status_msg = "Conflict";
    else if (ctx.status_code == 413)
        status_msg = "Payload Too Large";
    else if (ctx.status_code == 501)
        status_msg = "Not Implemented";
    else if (ctx.status_code == 502)
        status_msg = "Bad Gateway";
    else if (ctx.status_code == 504)
        status_msg = "Gateway Timeout";
    else if (ctx.status_code == 505)
        status_msg = "HTTP Version Not Supported";
    else
        status_msg = "Internal Server Error";

    ctx.response_headers = "HTTP/1.1 " + status_ss.str() + " " + status_msg + "\r\n";
    ctx.response_headers += "Content-Type: " + mimeType + "\r\n";
    ctx.response_headers += "Content-Length: " + len_ss.str() + "\r\n";
    ctx.response_headers += "\r\n";
}

//Function that takes type of file eg:(.html) and convert it to the output given for browser to identify send code
std::string MimeTypeSetter(std::string &full_path) {
    size_t dotIndex = full_path.find_last_of(".");
    if (dotIndex == std::string::npos)
        return ("text/plain"); //default
    std::string type = full_path.substr(dotIndex);

    static std::map<std::string, std::string> mime_map;
    if (mime_map.empty())// map of types
    {
        mime_map[".html"] = "text/html";
        mime_map[".htm"] = "text/html";
        mime_map[".css"] = "text/css";
        mime_map[".js"] = "application/javascript";
        mime_map[".json"] = "application/json";
        mime_map[".png"] = "image/png";
        mime_map[".jpg"] = "image/jpeg";
        mime_map[".jpeg"] = "image/jpeg";
        mime_map[".gif"] = "image/gif";
        mime_map[".ico"] = "image/x-icon";
        mime_map[".txt"] = "text/plain";
    }

    std::map<std::string, std::string>::iterator it = mime_map.find(type);
    if (it != mime_map.end())
        return (it->second);
    return ("text/plain");
}

//Root + Path = full path eg: (/public + /index.html = /public/index.html)
std::string RootPathJoin(std::string path, std::string root) {
    std::string joinedString;
    if (path == "/" || path == "")
    {
        joinedString = "./" + root + "/index.html";
        return (joinedString);
    }
    if (path[0] == '/')
        joinedString = "./" + root + path;
    else
        joinedString = "./" + root + "/" + path;
    return (joinedString);
}

//Checker for errors while opening file
void    FileCheck(const std::string fullPath, ClientContext& ctx) {
    struct stat file_info;
    if (stat(fullPath.c_str(), &file_info) == -1)
    {
        int error_code = errno;
        if (error_code == ENOENT)
            ctx.status_code = 404; //No such file or directory
        else if (error_code == EACCES)
            ctx.status_code = 403; //Permission denied
        else
            ctx.status_code = 500; //Sys error
        return ;
    }
    if (!S_ISREG(file_info.st_mode))
    {
        ctx.status_code = 403; //Directory
        return ;
    }
    ctx.status_code = 200; //Correct
}

//Opening and reading file and copying text to response
void    OpenAndReadFile(std::string full_path, ClientContext &ctx) {
    int fd = open(full_path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        ctx.status_code = 500; //Sys error
        return ;
    }

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        ctx.response_body.append(buffer, bytes_read);
        std::cout << "file: ";
        std::cout.write(buffer, bytes_read);
        std::cout << std::endl;
    }
    close(fd);

    if (bytes_read == -1)
    {
        ctx.status_code = 500;
        ctx.response_body.clear();
        return ;
    }
    ctx.status_code = 200;
}

// Loading error pages to ClientContext or default if config doesn't contain specified any
void    errorPageGetter(ClientContext &ctx) {
    std::string full_path;
    int original_status = ctx.status_code;

    std::map <int, std::string>::iterator it = ctx.Config.error_pages.find(ctx.status_code);

    if (it != ctx.Config.error_pages.end())
    {
        full_path = "./" + ctx.Config.root + "/" + it->second;
        OpenAndReadFile(full_path, ctx);
        ctx.status_code = original_status;
    }
    else
    {
        full_path = "./" + ctx.Config.root + "/defaultErrorPage.html";
        OpenAndReadFile(full_path, ctx);
        ctx.status_code = original_status;
    }
}

// Checker is path safe
bool    isPathSafe(std::string full_path) {
    if (full_path.find("../") != std::string::npos || full_path.find("//") != std::string::npos)
        return (false);
    else
        return (true);
}