#include "../../includes/config/Parser.hpp"


//function to validate info from config file
void Parser::validateConf(const std::vector<ServerConfig> &servers)
{
	if (servers.empty())
	{
		throw std::runtime_error("Config file contains no server blocks!");
	}

	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerConfig &one_srv = servers[i];

		if (one_srv.server_name.empty())
			throw std::runtime_error("Server name cannot be empty.");

		if (one_srv.port <= 0 || one_srv.port > 65535)
			throw std::runtime_error("Invalid port number. It has to be number between 1 and 65535.");

		if (one_srv.host.empty())
			throw std::runtime_error("Server host cannot be empty.");

		if (one_srv.root.empty())
			throw std::runtime_error("Server root directory cannot be empty.");

		for (size_t j = 0; j < one_srv.locations.size(); j++)
		{
			const LocationConfig &one_loc = one_srv.locations[j];

			if (one_loc.path.empty())
				throw std::runtime_error("Location path cannot be empty."); // teoretycznie nie może być bo wparseServerBlock ustawiam że gdy kiedykolwiek path jset pusta to staje się /

			for (size_t t = 0; t < one_loc.allowed_methods.size(); t++)
			{
				const std::string &one_method = one_loc.allowed_methods[t];
				if (one_method != "GET" && one_method != "POST" && one_method != "DELETE")
					throw std::runtime_error("Invalid HTTP method: '" + one_method + "'. Allowed: GET, POST, DELETE.");
			}
			if (one_loc.return_code != 0) 
			{
                if (one_loc.return_code < 300 || one_loc.return_code > 399)
                    throw std::runtime_error("Invalid redirect code.");
                if (one_loc.return_url.empty())
                    throw std::runtime_error("Redirect return URL is missing.");

            }
			bool ext = !one_loc.cgi_ext.empty();
            bool path = !one_loc.cgi_path.empty();
            if (ext != path)
                throw std::runtime_error("CGI configuration incomplete. Both extension and path must be provided.");
			
		}
		if (i == 0)
		{
			for (size_t a = 0; a < servers.size(); ++a)
			{
				for (size_t b = a + 1; b < servers.size(); ++b)
				{
					if (servers[a].host == servers[b].host && servers[a].port == servers[b].port)
						throw std::runtime_error("Duplicate server configuration with identical host and port.");
				}
			}
		}
	}
}
