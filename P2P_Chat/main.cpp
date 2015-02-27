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
			cerr << err.what();
		}
	}

	ChatClient chat(port);

	try {
		return chat.loop();
	}
	catch (logic_error& err) {
		cerr << err.what() << endl;
		return 0;
	}
}