// OnvifTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "OnvifServer.h"

int main()
{
    try {
        // Maak een io_context object aan
        boost::asio::io_context ioc;

        // Maak een Onvif server object aan op poort 8080
        OnvifServer server(ioc, 8080);

        // Draai de io_context om de server te laten werken
        ioc.run();
    }
    catch (std::exception& e) {
        // Vang eventuele uitzonderingen en print ze naar de standaardfoutstroom
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}