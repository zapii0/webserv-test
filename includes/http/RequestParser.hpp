#ifndef REQUESTPARSER_HPP
#define REQUESTPARSER_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include "../ClientContext.hpp"

enum Result
{
	PARSE_INCOMPLETE,
	PARSE_COMPLETE,
	PARSE_ERROR
};

class RequestParser
{
	private:
		std::string _raw_request;
		bool _is_valid;

	public:
		RequestParser(const std::string &request); //clientContext.raw_buffer ClientContext
		~RequestParser();

		//getter is valid

		Result parseRequest(ClientContext &client);

		Result parseFirstLine(ClientContext &client);
		std::vector<std::string> splitBySpace(const std::string &line) const;

		Result parseHeaders(ClientContext &client);
		std::vector<std::string> splitLines(const std::string &block) const;

		Result parseBody(ClientContext &client);
		Result parseChunkedBody(ClientContext &client, std::size_t body_start);
};

#endif