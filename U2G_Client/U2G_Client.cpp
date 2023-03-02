#include <iostream>
#include <string>

//RakNet includes:
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"

//IMPORTANT: This should stay in sync with the plugin project. Increment the netversion if you make changes to this enum or the serialization of packets.
#define NET_VERSION = 1;

enum class Messages : unsigned int {
	ID_STREAM_ON_PORT = ID_USER_PACKET_ENUM + 1, //this tells us what port to initialize our RTSP stream on. 
	ID_IMAGE_DATA, //new frame data for us to encode,
	ID_READY_FOR_DATA, //empty packet outside of the ID, just to show that we're ready for incoming image data.
};

struct StreamSettings {
	int port = 2002; //port the RTSP stream will run on
	RakNet::RakString codec; //the video codec we're supposed to use
	bool useHardwareEncoder; //try to use the hardware (NVENC) encoder if available; if not we'll fall back to the software implementation. 
};

using namespace RakNet;

int main(void) {
	RakPeerInterface* peer = RakPeerInterface::GetInstance();
	Packet* packet;
	SocketDescriptor socket;
	peer->Startup(1, &socket, 1);

	//Connect to the server:
	peer->Connect("127.0.0.1", 3001, "U2GPass", 7); //TODO: make ip & port read from a configuration file. 

	//Our main networking loop
	bool run = true;
	while (run) {
		for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) {
			switch (packet->data[0]) {
			case ID_DISCONNECTION_NOTIFICATION || ID_CONNECTION_LOST:
				std::cout << "Disconnected." << std::endl;
				run = false;
				break;

			case ID_CONNECTION_REQUEST_ACCEPTED:
			{
				std::cout << "Our connection request has been accepted. We are now connected to the Unity Rendering Plugin's Server." << std::endl;
				break;
			}

			case ID_NO_FREE_INCOMING_CONNECTIONS:
				std::cout << "Server has no more free slots for us to connect to." << std::endl;
				run = false;
				break;

			case ID_CONNECTION_ATTEMPT_FAILED:
				std::cout << "Connection timed out." << std::endl;
				run = false;
				break;

			default:
				std::cout << "Message with ID: " << int(packet->data[0]) << " has arrived." << std::endl;
			}
		}
	}

	//Clean up
	RakNet::RakPeerInterface::DestroyInstance(peer);
	return 0;
}