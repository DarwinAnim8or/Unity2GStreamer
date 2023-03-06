#pragma once
#include <iostream>
#include <string>
#include <map>

//RakNet includes:
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"

enum class Messages : unsigned int {
	ID_STREAM_SETTINGS = ID_USER_PACKET_ENUM + 1, //this tells us what port to initialize our RTSP stream on. 
	ID_HANDSHAKE,
	ID_HANDSHAKE_RESPONSE,
	ID_IMAGE_DATA, //new frame data for us to encode,
	ID_READY_FOR_DATA, //empty packet outside of the ID, just to show that we're ready for incoming image data.
};

struct StreamSettings {
	int port = 2002; //port the RTSP stream will run on
	RakNet::RakString codec; //the video codec we're supposed to use
	bool useHardwareEncoder; //try to use the hardware (NVENC) encoder if available; if not we'll fall back to the software implementation. 
};

using namespace RakNet;

class CCTVServer {
public:
	CCTVServer(int port = 3001);
	~CCTVServer();

	void Update(); //updates our rakPeer and checks for new messages from clients

	void SendHandshakeResponse(bool success, RakNetGUID& clientGUID);
	void SendNewFrameToEveryone(unsigned char* bytes, size_t size);
	void SendNewFrameToRemoteGstreamer(RakNetGUID client, const char* bytes, size_t size);
	void SendStreamSettings(const StreamSettings& settings);
	void Disconnect(RakNetGUID client);

private:
	std::map<uint32_t, RakNetGUID> m_Clients;
	int m_Port;

	RakNet::RakPeerInterface* m_Peer;
	RakNet::SocketDescriptor m_Socket;
	Packet* m_Packet;
};