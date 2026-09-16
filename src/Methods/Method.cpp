#include "../../includes/Methods.hpp"

void ExecMethods(ClientContext &ctx) {
    std::string script_path, interpreter;
    if (isCgiRequest(ctx, script_path, interpreter)) {
        ExecCgi(ctx, script_path, interpreter);
        return;
    }

    if (ctx.method == "GET") {
        GetMethod(ctx);
    } else if (ctx.method == "POST") {
        PostMethod(ctx);
    } else if (ctx.method == "DELETE") {
        DeleteMethod(ctx);
    } else {
        ctx.status_code = 501;
        errorPageGetter(ctx);
        BuildHeaders(ctx, ctx.path);
    }
}