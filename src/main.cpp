#include "../includes/config/Parser.hpp"
#include "../includes/ClientContext.hpp"
#include "../includes/http/RequestParser.hpp"
#include "../includes/server/ServerSocket.hpp"
#include "../includes/server/Poller.hpp"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config.conf>" << std::endl;
		return 1;
	}
	Parser parser(argv[1]);

	std::vector<ServerConfig> servers = parser.parse();

	std::cout << "Successfully parsed " << servers.size() << " server(s)!\n" << std::endl;
				
	Poller	PollManager(servers);
	PollManager.runPollLoop();
	return 0;
}