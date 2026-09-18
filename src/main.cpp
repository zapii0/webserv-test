#include "../includes/config/Parser.hpp"
#include "../includes/ClientContext.hpp"
#include "../includes/http/RequestParser.hpp"
#include "../includes/server/ServerSocket.hpp"
#include "../includes/server/Poller.hpp"

// (Twoje dotychczasowe #include)
#include <string>

int main(int argc, char **argv)
{
    std::string config_file;

    if (argc == 2)
    {
        config_file = argv[1];
    }
    else if (argc == 3 && std::string(argv[1]) == "-c")
    {
        config_file = argv[2]; // Obsługa flagi -c dla testera
    }
    else
    {
        std::cerr << "Usage: " << argv[0] << " [-c] <config.conf>" << std::endl;
        return 1;
    }

    Parser parser(config_file);
	try
    {
        std::vector<ServerConfig> servers = parser.parse();
        std::cout << "Successfully parsed " << servers.size() << " server(s)!\n" << std::endl;
                    
        Poller  PollManager(servers);
        PollManager.runPollLoop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1; // <--- DODAJ TO (informuje tester, że serwer się wywrócił)
    }
    
    return 0;
}