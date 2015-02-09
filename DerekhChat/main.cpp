#include "chat_client.h"
#include <boost/lexical_cast.hpp>

int main(int argc, char ** argv)
{
	int port = 54321;

	if (argc > 1) {
		try {
			port = boost::lexical_cast<int>(argv[1]);
		}
		catch (const boost::bad_lexical_cast& err) {
			std::cerr << err.what();
		}
	}

	ChatClient chat(port);

	try {
		return chat.loop();
	}
	catch (std::logic_error& err) {
		std::cerr << err.what() << std::endl;
		return 0;
	}
}