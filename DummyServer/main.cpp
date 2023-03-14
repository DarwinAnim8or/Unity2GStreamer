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

	std::cout << "THIS SERVER IS ONLY MEANT FOR DEBUGGING PURPOSES. IT WILL ONLY SEND *GENERATED* IMAGE DATA." << std::endl;

	//Create server:
	g_Server = new CCTVServer(3001);

	while (true) {
		g_Server->Update();

		auto image = g_Server->GenerateRandomRGBAImage(1280, 720);
		g_Server->SendNewFrameToEveryone(image.data.c_str(), image.size);

		RakSleep(30);
	}

	return 0;
}