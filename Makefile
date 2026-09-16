NAME = webserv
SRCS = src/config/Parser.cpp src/config/ValidateConfig.cpp src/server/Poller.cpp src/server/ServerSocket.cpp src/main.cpp src/http/RequestParser.cpp $(addprefix src/Methods/, Method.cpp MethodGet.cpp MethodPost.cpp MethodDelete.cpp CgiHandler.cpp)
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re