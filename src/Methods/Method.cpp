#include "../../includes/Methods.hpp"

void    ExecMethods(ClientContext &ctx) {
    if (ctx.method == "GET")
    {
        GetMethod(ctx);
    }
    else if (ctx.method == "POST")
    {
        PostMethod(ctx);
    }
//    else if (ctx.method == "DELETE")
//    {
//
//    }
}