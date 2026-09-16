#include "../../includes/Methods.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <cerrno>
#include <cstdio>

void DeleteMethod(ClientContext &ctx) {
    std::string fullPath = RootPathJoin(ctx.path, ctx.Config.root);

    if (!isPathSafe(fullPath)) {
        ctx.status_code = 400;
        errorPageGetter(ctx);
        BuildHeaders(ctx, fullPath);
        return;
    }
    if (access(fullPath.c_str(), F_OK) != 0) {
        size_t file_index = ctx.path.find_last_of("/") + 1;
        std::string fileName = ctx.path.substr(file_index);
        std::string upload_dir = ctx.Config.root + "/upload";
        std::string uploadPath = RootPathJoin(fileName, upload_dir);
        if (access(uploadPath.c_str(), F_OK) == 0) {
            fullPath = uploadPath;
        }
    }
    struct stat file_info;
    if (stat(fullPath.c_str(), &file_info) == -1) {
        if (errno == ENOENT)
            ctx.status_code = 404;
        else if (errno == EACCES)
            ctx.status_code = 403;
        else
            ctx.status_code = 500;
        errorPageGetter(ctx);
        BuildHeaders(ctx, fullPath);
        return;
    }
    if (S_ISDIR(file_info.st_mode)) {
        ctx.status_code = 403;
        errorPageGetter(ctx);
        BuildHeaders(ctx, fullPath);
        return;
    }
    if (std::remove(fullPath.c_str()) == 0) {
        ctx.status_code = 200;
        ctx.response_body = "<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>";
    } else {
        if (errno == EACCES || errno == EPERM)
            ctx.status_code = 403;
        else if (errno == ENOENT)
            ctx.status_code = 404;
        else
            ctx.status_code = 500;
        errorPageGetter(ctx);
    }
    BuildHeaders(ctx, fullPath);
}