#include "CCTVServer.h"

CCTVServer::CCTVServer(int port) {
	m_Port = port;

	//Create our server:
	m_Peer = RakNet::RakPeerInterface::GetInstance();

	//Configure our Socket:
	m_Socket.port = 3001; //3001 was chosen because ports under 2048 may be set up under Linux as reserved for 'root' only.
	m_Socket.socketFamily = AF_INET; //IPv4, since we're on a LAN anyway

	//Startup:
	m_Peer->Startup(2048, &m_Socket, 1);
	m_Peer->SetIncomingPassword("U2GPass", 7);
	m_Peer->SetMaximumIncomingConnections(2048); //Was also specified in startup-- but the RakNet documentation does this so I'm copying it.

	m_Packet = nullptr;

	std::cout << "Started server on port: " << port << std::endl;
}

CCTVServer::~CCTVServer() {
	if (!m_Peer) return;
	if (m_Packet) m_Peer->DeallocatePacket(m_Packet);

	m_Peer->Shutdown(1000);
	RakNet::RakPeerInterface::DestroyInstance(m_Peer);
}

void CCTVServer::Update() {
	m_Packet = m_Peer->Receive();
	if (!m_Packet || m_Packet->bitSize == 0) return;

	switch (m_Packet->data[0]) {
	case ID_DISCONNECTION_NOTIFICATION || ID_CONNECTION_LOST:
		std::cout << "A client disconnected." << std::endl;
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
	{
		std::cout << "Our connection request has been accepted. We are now connected to the Unity Rendering Plugin's Server." << std::endl;
		break;
	}

	case ID_NO_FREE_INCOMING_CONNECTIONS:
		std::cout << "Server has no more free slots for us to connect to." << std::endl;
		break;

	case ID_CONNECTION_ATTEMPT_FAILED:
		std::cout << "Connection timed out." << std::endl;
		break;

	default:
		std::cout << "Message with ID: " << int(m_Packet->data[0]) << " has arrived." << std::endl;
	}

	m_Peer->DeallocatePacket(m_Packet);
}

void CCTVServer::SendNewFrameToRemoteGstreamer(SystemAddress* sysAddr, const char* bytes, size_t size) {
	BitStream bitStream;
	bitStream.Write(Messages::ID_IMAGE_DATA);
	bitStream.Write(size);
	
	for (size_t i = 0; i < size; i++) {
		bitStream.Write(bytes[i]);
	}

	//TODO: Figure out which level of reliability is needed
	//TODO: Implement a system that uses the orderingchannel, rather than TCP-like everything being channel 0.
	m_Peer->Send(&bitStream, PacketPriority::HIGH_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0, *sysAddr, false);
}

void CCTVServer::SendStreamSettings(const StreamSettings& settings) {
}

void CCTVServer::Disconnect(SystemAddress sysAddr) {
	//this "kicks" a client from our server:
	for (auto addr : m_Clients) {
		if (addr.second == &sysAddr) {
			//m_Peer->CloseConnection(addr.second->guid, true);
		}
	}
}
