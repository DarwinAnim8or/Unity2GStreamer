#include <iostream>
#include "CCTVServer.h"
#include "RakSleep.h"

static CCTVServer* g_Server = nullptr;

int main() {
	//Print a big fat warning:
	std::cout << "######  ####### ######  #     #  #####     ####### #     # #       #     #" << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # ##    # #        #   #  " << std::endl;
	std::cout << "#     # #       #     # #     # #          #     # # #   # #         # #   " << std::endl;
	std::cout << "#     # #####   ######  #     # #  ####    #     # #  #  # #          #    " << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # #   # # #          #    " << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # #    ## #          #    " << std::endl;
	std::cout << "######  ####### ######   #####   #####     ####### #     # #######    #   " << std::endl << std::endl;

	std::cout << "THIS SERVER IS ONLY MEANT FOR DEBUGGING PURPOSES. IT WILL NOT SEND IMAGE DATA." << std::endl;

	//Create server:
	g_Server = new CCTVServer(3001);

	while (true) {
		g_Server->Update();
		RakSleep(30);
	}

	return 0;
}