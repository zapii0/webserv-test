#include "../../includes/config/Parser.hpp"

Parser::Parser(const std::string &filepath) : _filepath(filepath) {}

Parser::~Parser() {}

// funkcja sprawdzająca czy plik config na pewno ma rozszerzenie .conf
std::string Parser::isConfigFile(std::string &path)
{
	const std::string ext = ".conf";
	if (path.size() >= ext.size() &&
		path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
		return path;

	throw std::runtime_error("file '" + path + "' is not a config file.");
}

// funkcja usuwająca białe znaki na początku i na końcu stringa
std::string Parser::trim(const std::string &str)
{
	size_t first_index = str.find_first_not_of(" \t\r\n");
	if (first_index == std::string::npos)
		return "";
	size_t last_index = str.find_last_not_of(" \t\r\n");

	return str.substr(first_index, (last_index - first_index + 1));
}
// funkcja parsująca jeden blok server z pliku konfiguracyjnego
void Parser::parseLocationBlock(std::ifstream &file, LocationConfig &location)
{
	std::string line;
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty())
			continue;

		if (line == "}") // jak dojdzie do końca server{}
			break;

		std::string word;
		size_t space = line.find(' ');
		if (space != std::string::npos)
			word = line.substr(0, space);
		else
			word = line;

		if (word == "default")
		{
			std::string new_str = line.substr(space + 1);
			location.default_file = trim(new_str);
		}
		else if (word == "root")
		{
			std::string new_str = line.substr(space + 1);
			new_str = trim(new_str);
			if (!new_str.empty() && new_str[new_str.size() - 1] == ';')
				new_str.resize(new_str.size() - 1);
			location.root = trim(new_str);
		}
		else if (word == "allowed_methods")
		{
			std::string new_str = line.substr(space + 1);
			new_str = trim(new_str);
			while (!new_str.empty())
			{
				size_t space = new_str.find(' ');
				std::string one_method;
				if (space != std::string::npos)
				{
					one_method = new_str.substr(0, space);
					new_str = new_str.substr(space + 1);
					new_str = trim(new_str);
				}
				else // ostatnia metoda
				{
					one_method = new_str;
					new_str.clear();
				}
				if (!one_method.empty())
					location.allowed_methods.push_back(one_method);
			}
		}
		else if (word == "autoindex")
		{
			std::string new_str = line.substr(space + 1);
			new_str = trim(new_str);
			if (!new_str.empty() && new_str[new_str.size() - 1] == ';') //
				new_str.resize(new_str.size() - 1); //
			new_str = trim(new_str); //
			if (new_str == "off")
				continue;
			else if (new_str == "on")
				location.autoindex = true;
			else
				throw std::runtime_error("Autoindex can only be \"on\" or \"off\"");

		}
		else if (word == "return")
		{
			std::string new_str = line.substr(space + 1);
			new_str = trim(new_str);
			size_t space = new_str.find(' ');
			if (space != std::string::npos)
			{
				std::string code = new_str.substr(0, space);
					location.return_code = atoi(code.c_str());

				std::string url_str = new_str.substr(space + 1);
				url_str = trim(url_str);
				location.return_url = url_str;
			}
		}
		else if (word == "cgi")
		{
			std::string new_str = trim(line.substr(space + 1));
			size_t sep = new_str.find(' ');
			if (sep == std::string::npos)
			{
				location.cgi_ext = trim(new_str);
				location.cgi_path.clear();
			}
			else
			{
				location.cgi_ext = trim(new_str.substr(0, sep));
				location.cgi_path = trim(new_str.substr(sep + 1));
			}
		}
		else if (word == "client_max_body_size")
		{
			std::string size_str = trim(line.substr(space + 1));
			if (!size_str.empty() && size_str[size_str.size() - 1] == ';')
				size_str.resize(size_str.size() - 1);
			location.client_max_body_size = static_cast<size_t>(atoi(trim(size_str).c_str()));
		}
	}
}

// funkcja parsująca jeden blok location z bloku server z pliku konfiguracyjnego
void Parser::parseServerBlock(std::ifstream &file, ServerConfig &server)
{
	std::string line;

	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty())
			continue;

		if (line == "}") // jak dojdzie do końca server{}
			break;

		std::string word;
		size_t space = line.find(' ');
		if (space != std::string::npos)
			word = line.substr(0, space);
		else
			word = line;

		if (word == "server_name")
		{
			std::string new_word;
			if (space != std::string::npos)
				new_word = line.substr(space + 1);
			else
				new_word = "";
			if (!new_word.empty() && new_word[new_word.size() - 1] == ';')
				new_word.resize(new_word.size() - 1);
			server.server_name = trim(new_word);
		}
		else if (word == "listen")
		{
			std::string port_str = line.substr(space + 1);
			server.port = atoi(port_str.c_str());
		}
		else if (word == "host")
		{
			std::string new_word;
			if (space != std::string::npos)
				new_word = line.substr(space + 1);
			else
				new_word = "";
			if (!new_word.empty() && new_word[new_word.size() - 1] == ';')
				new_word.resize(new_word.size() - 1);
			server.host = trim(new_word);
		}
		else if (word == "client_max_body_size")
		{
			std::string size_str = line.substr(space + 1);
			server.client_max_body_size = atoi(size_str.c_str());
		}
		else if (word == "root")
		{
			std::string new_word;
			if (space != std::string::npos)
				new_word = line.substr(space + 1);
			else
				new_word = "";
			if (!new_word.empty() && new_word[new_word.size() - 1] == ';')
				new_word.resize(new_word.size() - 1);
			server.root = trim(new_word);
		}
		else if (word == "error_page")
		{
			std::string new_word = line.substr(space + 1);
			size_t next_space = new_word.find(' ');
			if (next_space != std::string::npos)
			{
				int code = atoi(new_word.substr(0, next_space).c_str());
				std::string file_path = new_word.substr(next_space + 1);
				std::string trimmed_path = trim(file_path);
				if (!trimmed_path.empty() && trimmed_path[trimmed_path.size() - 1] == ';')
					trimmed_path.erase(trimmed_path.size() - 1); // usunięcie średnika

				server.error_pages[code] = trimmed_path;
			}
		}
		else if (word == "location")
		{
			std::string path = trim(line.substr(space + 1));

			if (!path.empty() && path[path.length() - 1] == '{')
				path = trim(path.substr(0, path.length() - 1));
			if (path.empty())
				path = "/";

			LocationConfig location;
			location.path = path;
			parseLocationBlock(file, location);
			server.locations.push_back(location);
		}
	}
}

std::vector<ServerConfig> Parser::parse()
{
	std::vector<ServerConfig> servers;
	isConfigFile(_filepath);
	std::ifstream file(_filepath.c_str());
	if (!file.is_open())
	{
		throw std::runtime_error("Could not open configuration file: " + _filepath);
	}

	std::string line;
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty())
			continue;
		if (line == "server {")
		{
			ServerConfig server;
			parseServerBlock(file, server);
			servers.push_back(server);
		}
	}
	validateConf(servers);
	file.close();
	return servers;
}
