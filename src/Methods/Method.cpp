#include "../../includes/Methods.hpp"
#include <algorithm>
#include <iostream>
void DebugClientContext(const ClientContext *ctx);

void ExecMethods(ClientContext &ctx) {
    ClientContextRoute(&ctx);
    DebugClientContext(&ctx); // Wywołanie funkcji debugującej
    if (ctx.status_code == 301) {
        BuildHeaders(ctx, ctx.path);
        return;
    }
    if (ctx.status_code != 200 && ctx.status_code != 0) {
        errorPageGetter(ctx);
        BuildHeaders(ctx, ctx.path);
        return;
    }
    if (ctx.method == "POST") {
        std::string script_path;
        std::string interpreter;
        if (isCgiRequest(ctx, script_path, interpreter)) {
            ExecCgi(ctx, script_path, interpreter);
            return;
        }
    }
    if (ctx.method == "GET") {
        GetMethod(ctx);
    } 
    else if (ctx.method == "POST") {
        PostMethod(ctx);
    } 
    else if (ctx.method == "DELETE") {
        DeleteMethod(ctx);
    } 
    else {
        ctx.status_code = 501;
        errorPageGetter(ctx);
        BuildHeaders(ctx, ctx.path);
    }
}

void ClientContextRoute(ClientContext *ctx) {
    size_t longest_match = 0;
    int matched_index = -1;

    // 1. Szukanie najdłuższego dopasowania location
    for (size_t i = 0; i < ctx->Config.locations.size(); ++i) {
        const std::string &location_path = ctx->Config.locations[i].path;
        bool matches = ctx->path.find(location_path) == 0;
        if (!matches && location_path.length() > 1 &&
            location_path[location_path.length() - 1] == '/' &&
            ctx->path == location_path.substr(0, location_path.length() - 1))
            matches = true;
        if (matches && location_path != "/" &&
            location_path[location_path.length() - 1] != '/' &&
            ctx->path.length() > location_path.length() &&
            ctx->path[location_path.length()] != '/') {
            matches = false;
        }
        if (matches) {
            if (location_path.length() > longest_match) {
                longest_match = location_path.length();
                matched_index = i;
            }
        }
    }

    std::string active_root = ctx->Config.root;
    std::string default_file = "";
    std::vector<std::string> allowed_methods;

    // 2. Jeśli znaleziono location, nadpisujemy domyślne zmienne
    if (matched_index != -1) {
        const std::string &location_path = ctx->Config.locations[matched_index].path;
        if (location_path.length() > 1 &&
            location_path[location_path.length() - 1] == '/' &&
            ctx->path == location_path.substr(0, location_path.length() - 1)) {
            ctx->status_code = 301;
            ctx->redirect_location = location_path;
            return;
        }
        if (!ctx->Config.locations[matched_index].root.empty()) {
            active_root = ctx->Config.locations[matched_index].root;
        }
        default_file = ctx->Config.locations[matched_index].default_file;
        allowed_methods = ctx->Config.locations[matched_index].allowed_methods;
        if (ctx->Config.locations[matched_index].client_max_body_size > 0)
            ctx->Config.client_max_body_size =
                ctx->Config.locations[matched_index].client_max_body_size;
    }
    if (ctx->path == "/" && default_file.empty())
        default_file = "index.html";

    // 3. Weryfikacja metody natychmiast po znalezieniu ruleset'u
    bool cgi_request = false;
    for (size_t i = 0; i < ctx->Config.locations.size(); ++i) {
        const std::string &extension = ctx->Config.locations[i].cgi_ext;
        if (!extension.empty() && ctx->path.length() >= extension.length() &&
            ctx->path.substr(ctx->path.length() - extension.length()) == extension) {
            cgi_request = true;
            break;
        }
    }
    if (!cgi_request && !allowed_methods.empty()) {
        if (std::find(allowed_methods.begin(), allowed_methods.end(), ctx->method) == allowed_methods.end()) {
            ctx->status_code = 405; // Method Not Allowed
            return; // Przerywamy dalsze procesowanie
        }
    }

    // 4. Transformacja ścieżki (cięcie aliasu i klejenie roota)
    std::string uri_remainder = "";
    if (longest_match > 0 && matched_index != -1) {
        // Usuwamy z zapytania część pasującą do location (np. usuwamy "/directory/")
        uri_remainder = ctx->path.substr(longest_match);
    } else {
        uri_remainder = ctx->path;
    }

    if (uri_remainder.empty())
        uri_remainder = "/";

    // Korekta slashy, żeby nie skleić np. "YoupiBanane" i "nop/file" bez slasha
    if (!active_root.empty() && active_root[active_root.length() - 1] == '/') {
        active_root.erase(active_root.length() - 1);
    }
    if (!uri_remainder.empty() && uri_remainder[0] != '/') {
        uri_remainder = "/" + uri_remainder;
    }

    std::string physical_path = active_root + uri_remainder;
    if (physical_path.empty() || physical_path[0] != '.')
        physical_path = "./" + physical_path;

    struct stat path_info;
    if (!physical_path.empty() && physical_path[physical_path.length() - 1] != '/' &&
        stat(physical_path.c_str(), &path_info) == 0 && S_ISDIR(path_info.st_mode)) {
        ctx->status_code = 301;
        ctx->redirect_location = ctx->path + "/";
        return;
    }

    // 5. Obsługa plików domyślnych dla katalogów
    // (Możesz tu użyć stat(), by sprawdzić czy physical_path to folder)
    if (!physical_path.empty() && physical_path[physical_path.length() - 1] == '/') {
        if (!default_file.empty()) {
            physical_path += default_file;
        } else {
            // Logika autoindex lub błąd 403 Forbidden
        }
    }

    // 6. Wykrywanie CGI (.bla)
    if (matched_index != -1 && !ctx->Config.locations[matched_index].cgi_ext.empty()) {
        size_t ext_pos = physical_path.find(ctx->Config.locations[matched_index].cgi_ext);
        if (ext_pos != std::string::npos) {
            // Ustaw flagę w kontekście, by później wywołać ExecCgi
            // np. ctx->is_cgi = true;
        }
    }

    // Keep the URI in ctx->path for CGI environment variables and retain
    // the resolved filesystem path separately for regular file handling.
    ctx->resolved_path = physical_path;
}
#include <iostream>
#include <map>
#include <vector>

void DebugClientContext(const ClientContext *ctx) {
    std::cout << "\n========== DEBUG: CLIENT CONTEXT ==========\n";
    
    std::cout << "[1. ZAPYTANIE HTTP (Publiczne pola ClientContext)]\n";
    std::cout << "Method:      " << ctx->method << "\n";
    std::cout << "Path:        " << ctx->path << "\n";
    std::cout << "Status Code: " << ctx->status_code << "\n";
    
    std::cout << "Headers:\n";
    for (std::map<std::string, std::string>::const_iterator it = ctx->headers.begin(); it != ctx->headers.end(); ++it) {
        std::cout << "  - " << it->first << ": " << it->second << "\n";
    }

    std::cout << "\n[2. AKTYWNY SERWER (ServerConfig)]\n";
    std::cout << "Server Name: " << ctx->Config.server_name << "\n";
    std::cout << "Host:        " << ctx->Config.host << "\n";
    std::cout << "Port:        " << ctx->Config.port << "\n";
    std::cout << "Global Root: " << ctx->Config.root << "\n";
    std::cout << "Max Body:    " << ctx->Config.client_max_body_size << "\n";

    std::cout << "\n[3. DOSTĘPNE LOKALIZACJE DLA TEGO SERWERA (" << ctx->Config.locations.size() << ")]\n";
    for (size_t i = 0; i < ctx->Config.locations.size(); ++i) {
        std::cout << "  Location [" << i << "]: " << ctx->Config.locations[i].path << "\n";
        
        std::cout << "    - Root:            " << (ctx->Config.locations[i].root.empty() ? "(brak - dziedziczy globalny)" : ctx->Config.locations[i].root) << "\n";
        std::cout << "    - Default File:    " << (ctx->Config.locations[i].default_file.empty() ? "(brak)" : ctx->Config.locations[i].default_file) << "\n";
        std::cout << "    - Autoindex:       " << (ctx->Config.locations[i].autoindex ? "ON" : "OFF") << "\n";
        std::cout << "    - CGI Ext / Path:  " << (ctx->Config.locations[i].cgi_ext.empty() ? "(brak)" : ctx->Config.locations[i].cgi_ext) << " -> " << ctx->Config.locations[i].cgi_path << "\n";
        std::cout << "    - Return Code/URL: " << ctx->Config.locations[i].return_code << " -> " << ctx->Config.locations[i].return_url << "\n";
        
        std::cout << "    - Allowed Methods: ";
        if (ctx->Config.locations[i].allowed_methods.empty()) {
            std::cout << "(wszystkie/domyślne)";
        } else {
            for (size_t j = 0; j < ctx->Config.locations[i].allowed_methods.size(); ++j) {
                std::cout << ctx->Config.locations[i].allowed_methods[j] << " ";
            }
        }
        std::cout << "\n\n";
    }
    std::cout << "===========================================\n\n";
}
