#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstdlib>

// struktura do przechowywania pojedynczej ścieżki
struct LocationConfig
{
	std::string path; // ścieżka do danego pliku
	std::string default_file; // nazwa pliku
	std::vector<std::string> allowed_methods; // metody
	bool autoindex;
	int return_code;
	std::string return_url; // gdy ma przenieść do kompletnie innej strony

	std::string cgi_ext;
	std::string cgi_path;

	LocationConfig() : autoindex(false), return_code(0) {}// konstruktor domyślny
};

// struktura na przechowywanie pojedynczego servera
struct ServerConfig
{
	std::string server_name;
	int port; // listen
	std::string host; // IP
	size_t client_max_body_size;
	std::string root;
	std::map<int, std::string> error_pages;
	std::vector<LocationConfig> locations;

	ServerConfig() : port(80), client_max_body_size(1048576) {} // wartości domyślne w razie jakby w pliku konfiguracyjnym nie było informacji o tym
};

class Parser
{
	private:
		std::string _filepath;
		std::string trim(const std::string &str);
		void parseServerBlock(std::ifstream &file, ServerConfig &server);
		void parseLocationBlock(std::ifstream &file, LocationConfig &location);

	public:
		Parser(const std::string &filepath); // ścieżka do pliku conf
		~Parser();

		std::string isConfigFile(std::string &path);
		std::vector<ServerConfig> parse(); // zwraca listę serverów z pliku conf
		void validateConf(const std::vector<ServerConfig> &servers);
};

#endif