#include "../../includes/http/RequestParser.hpp"

RequestParser::RequestParser(const std::string &request) : _raw_request(request), _is_valid(true)
{
}

RequestParser::~RequestParser() {}

Result RequestParser::parseChunkedBody(ClientContext &client, std::size_t body_start)
{
    std::string assembled_body = "";
    std::size_t current_pos = body_start;

    while (current_pos < _raw_request.length())
    {
        std::size_t line_end = _raw_request.find("\r\n", current_pos);
        if (line_end == std::string::npos)
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_ERROR;
        }

        std::string hex_size_str = _raw_request.substr(current_pos, line_end - current_pos);
        std::size_t chunk_size = 0;
        std::stringstream ss;
        ss << std::hex << hex_size_str;
        ss >> chunk_size;

        if (ss.fail())
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_ERROR;
        }

        current_pos = line_end + 2;

        if (chunk_size == 0)
            break;
        if (client.Config.client_max_body_size > 0 && assembled_body.length() + chunk_size > client.Config.client_max_body_size)
        {
            _is_valid = false;
            client.status_code = 413;
            return PARSE_ERROR;
        }

        if (current_pos + chunk_size + 2 > _raw_request.length())
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_INCOMPLETE;
        }

        assembled_body.append(_raw_request.substr(current_pos, chunk_size));
        current_pos += chunk_size + 2;
    }

    client.request_body = assembled_body;
    return PARSE_COMPLETE;
}

Result RequestParser::parseBody(ClientContext &client)
{
    std::size_t headers_end = _raw_request.find("\r\n\r\n");
    if (headers_end == std::string::npos)
    {
        _is_valid = false;
        client.status_code = 400;
        return PARSE_INCOMPLETE;
    }

    std::size_t body_start = headers_end + 4;

    if (client.headers.find("Transfer-Encoding") != client.headers.end() &&
        client.headers["Transfer-Encoding"] == "chunked")
        return parseChunkedBody(client, body_start);

    if (client.headers.find("Content-Length") != client.headers.end())
    {
        int content_length = std::atoi(client.headers["Content-Length"].c_str());
        if (content_length < 0)
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_ERROR;
        }
        if (content_length == 0)
            return PARSE_COMPLETE;

        if (_raw_request.length() - body_start < (std::size_t)content_length)
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_INCOMPLETE;
        }
        else if (_raw_request.length() - body_start > (std::size_t)content_length)
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_ERROR;
        }
        if (client.Config.client_max_body_size > 0 && (std::size_t)content_length > client.Config.client_max_body_size)
        {
            _is_valid = false;
            client.status_code = 413;
            return PARSE_ERROR;
        }

        client.request_body = _raw_request.substr(body_start, content_length);
        return PARSE_COMPLETE;
    }

    return PARSE_COMPLETE;
}

std::vector<std::string> RequestParser::splitLines(const std::string &block) const
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    std::size_t end = block.find("\r\n");

    while (end != std::string::npos)
    {
        lines.push_back(block.substr(start, end - start));
        start = end + 2;
        end = block.find("\r\n", start);
    }

    if (start < block.length())
        lines.push_back(block.substr(start));

    return lines;
}

Result RequestParser::parseHeaders(ClientContext &client)
{
    std::size_t headers_start = _raw_request.find("\r\n");
    if (headers_start == std::string::npos)
    {
        _is_valid = false;
        client.status_code = 400;
        return PARSE_INCOMPLETE;
    }
    headers_start += 2;

    std::size_t headers_end = _raw_request.find("\r\n\r\n", headers_start);
    std::string headers_block;
    if (headers_end == std::string::npos)
        headers_block = _raw_request.substr(headers_start);
    else
        headers_block = _raw_request.substr(headers_start, headers_end - headers_start);

    std::vector<std::string> lines = splitLines(headers_block);
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        std::string line = lines[i];
        if (line.empty()) continue;

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            _is_valid = false;
            client.status_code = 400;
            return PARSE_ERROR;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        while (!value.empty() && value[0] == ' ') value.erase(0, 1);

        client.headers[key] = value;
    }
    return PARSE_INCOMPLETE;
}

std::vector<std::string> RequestParser::splitBySpace(const std::string &line) const
{
    std::vector<std::string> tokens;
    std::size_t start = 0;
    std::size_t end = line.find(' ');

    while (end != std::string::npos)
    {
        if (end > start)
            tokens.push_back(line.substr(start, end - start));
        start = end + 1;
        end = line.find(' ', start);
    }
    if (start < line.length())
        tokens.push_back(line.substr(start));

    return tokens;
}

Result RequestParser::parseFirstLine(ClientContext &client)
{
    std::size_t end_of_first_line = _raw_request.find("\r\n");
    if (end_of_first_line == std::string::npos)
    {
        _is_valid = false;
        client.status_code = 400;
        return PARSE_INCOMPLETE;
    }

    std::string first_line = _raw_request.substr(0, end_of_first_line);
    std::vector<std::string> tokens = splitBySpace(first_line);

    if (tokens.size() != 3)
    {
        _is_valid = false;
        client.status_code = 400;
        return PARSE_ERROR;
    }

    client.method = tokens[0];
    
    std::size_t qpos = tokens[1].find('?');
    if (qpos != std::string::npos)
    {
        client.path = tokens[1].substr(0, qpos); // ucina path na znaku '?'
        client.query_string = tokens[1].substr(qpos + 1); // wrzuca reszte do query_string
    }
    else
    {
        client.path = tokens[1];
        client.query_string = "";
    }

    // NORMALIZACJA ŚCIEŻKI: zamiana "//" na "/"
    std::size_t double_slash_pos;
    while ((double_slash_pos = client.path.find("//")) != std::string::npos)
    {
        client.path.erase(double_slash_pos, 1);
    }

    std::string http_version = tokens[2];

    if (client.method != "GET" && client.method != "POST" && client.method != "DELETE")
    {
        _is_valid = false;
        client.status_code = 501;
        return PARSE_ERROR;
    }

    if (http_version != "HTTP/1.1" && http_version != "HTTP/1.0")
    {
        _is_valid = false;
        client.status_code = 505;
        return PARSE_ERROR;
    }

    return PARSE_COMPLETE;
}

Result RequestParser::parseRequest(ClientContext &client)
{
    Result result;

    result = parseFirstLine(client);
    if (!_is_valid)
        return result;

    result = parseHeaders(client);
    if (!_is_valid)
        return result;

    result = parseBody(client);
    if (!_is_valid)
        return result;

    return PARSE_COMPLETE;
}