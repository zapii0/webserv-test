#ifndef METHODS_HPP
#define METHODS_HPP

#include "ClientContext.hpp"

class ClientContext;

void    ExecMethods(ClientContext &ctx);

// Method Get
void    BuildHeaders(ClientContext& ctx, std::string full_path);
bool    isPathSafe(std::string full_path);
void    errorPageGetter(ClientContext &ctx);
std::string RootPathJoin(std::string path, std::string root);
void    FileCheck(const std::string fullPath, ClientContext& ctx);
void    OpenAndReadFile(std::string full_path, ClientContext &ctx);
std::string MimeTypeSetter(std::string &full_path);
void    GetMethod(ClientContext& ctx);

// Method Post
void    PostMethod(ClientContext &ctx);
void    CreateAndWrite(std::string fullPath, ClientContext &ctx);

// Method Delete
void    DeleteMethod(ClientContext &ctx);

// CGI
bool    isCgiRequest(ClientContext &ctx, std::string &script_path, std::string &interpreter);
void    ExecCgi(ClientContext &ctx, const std::string &script_path, const std::string &interpreter);
void    parseCgiOutput(ClientContext &ctx);

#endif