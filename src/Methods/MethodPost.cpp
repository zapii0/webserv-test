#include "../../includes/Methods.hpp"
#include <string>

// int main(void)
// {
//     ClientContext ctx(5);
//     ctx.path = "index.html";
//     ctx.Config.root = "public";
//     ctx.response_body = "test";
//     ctx.Config.error_pages[404] = "error_404.html";
//     ctx.Config.error_pages[403] = "error_403.html";
//     ctx.Config.error_pages[500] = "error_500.html";
    
//     PostMethod(ctx);
//     //debug
//     return 0;
// }

void    PostMethod(ClientContext &ctx) {
    int file_index = ctx.path.find_last_of("/") + 1;
    std::string fileName = ctx.path.substr(file_index);
    std::string upload_dir = ctx.Config.root + "/upload";
    std::string fullPath = RootPathJoin(fileName, upload_dir);

    if (isPathSafe(fullPath))
        CreateAndWrite(fullPath, ctx);
    else
        ctx.status_code = 400;
    if (ctx.status_code == 201)
        ctx.response_body = "<html><body><h1>201 Created</h1><p>File saved successfully.</p></body></html>";
    else
        errorPageGetter(ctx);
    BuildHeaders(ctx, fullPath);
    std::cout << "POST Full Path: " << fullPath << std::endl;
    std::cout << "Status Code: " << ctx.status_code << std::endl;
}

void    CreateAndWrite(std::string fullPath, ClientContext &ctx) {
    int fd = open(fullPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd == -1)
    {
        if (errno == EEXIST)
            ctx.status_code = 409; // File exist
        else if (errno == EACCES)
            ctx.status_code = 403; // No acces
        else if (errno == ENOENT)
            ctx.status_code = 404; // Not Found
        else
            ctx.status_code = 500; // Internal Server Error
        return ;
    }
    ssize_t bytes_written = write(fd, ctx.request_body.c_str(), ctx.request_body.size());
    close(fd);
    if (bytes_written == -1 || (size_t)bytes_written != ctx.request_body.size())
    {
        ctx.status_code = 500;
        return ;
    }
    ctx.status_code = 201;
}
