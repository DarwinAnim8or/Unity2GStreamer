#include "CCTVServer.h"

//IMPORTANT: This should stay in sync with the client project. Increment the netversion if you make changes to this enum or the serialization of packets.
int NET_VERSION = 3;

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
	case ID_DISCONNECTION_NOTIFICATION:
	case ID_CONNECTION_LOST:
		std::cout << "A client disconnected." << std::endl;
		break;

	case ID_NEW_INCOMING_CONNECTION:
	{
		std::cout << "A client has connected, awaiting handshake." << std::endl;
		break;
	}

	case (int)Messages::ID_HANDSHAKE:
	{
		std::cout << "Handshake received" << std::endl;

		//Read the handshake for this client:
		RakNet::BitStream is(m_Packet->data, m_Packet->length, false);
		MessageID msgID;
		int netVersion = 0;
		is.Read(msgID);
		is.Read(netVersion);

		SendHandshakeResponse(netVersion == NET_VERSION, m_Packet->guid);

		if (netVersion == NET_VERSION) {
			m_Clients.insert(std::make_pair(m_Packet->systemAddress.GetPort(), m_Packet->guid));

			//TEMP: Tell them to start streaming:
			StreamSettings ss;
			ss.codec = "h264";
			ss.useHardwareEncoder = false;
			ss.port = 8558;
			SendStreamSettings(ss);
		}

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

void CCTVServer::SendHandshakeResponse(bool success, RakNetGUID& clientGUID) {
	BitStream bs;
	bs.Write((MessageID)Messages::ID_HANDSHAKE_RESPONSE);
	bs.Write(success);
	m_Peer->Send(&bs, PacketPriority::IMMEDIATE_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0, clientGUID, false);
}

void CCTVServer::SendNewFrameToEveryone(unsigned char* bytes, size_t size, int width, int height) {
	BitStream bitStream;
	bitStream.Write((MessageID)Messages::ID_IMAGE_DATA);
	bitStream.Write<unsigned int>(width);
	bitStream.Write<unsigned int>(height);
	bitStream.Write<unsigned int>((unsigned int)size);

	for (size_t i = 0; i < size; i++) {
		bitStream.Write(bytes[i]);
	}

	std::cout << bytes[0] << std::endl;
	m_Peer->Send(&bitStream, PacketPriority::HIGH_PRIORITY, PacketReliability::UNRELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
}

void CCTVServer::SendNewFrameToRemoteGstreamer(RakNetGUID client, const char* bytes, size_t size) {
	BitStream bitStream;
	bitStream.Write((MessageID)Messages::ID_IMAGE_DATA);
	bitStream.Write<unsigned int>((unsigned int)size);

	for (size_t i = 0; i < size; i++) {
		bitStream.Write(bytes[i]);
	}

	//TODO: Figure out which level of reliability is needed
	//TODO: Implement a system that uses the orderingchannel, rather than TCP-like everything being channel 0.
	m_Peer->Send(&bitStream, PacketPriority::HIGH_PRIORITY, PacketReliability::UNRELIABLE, 0, client, false);
}

void CCTVServer::SendStreamSettings(const StreamSettings& settings) {
	RakNet::BitStream bs;
	bs.Write((MessageID)Messages::ID_STREAM_SETTINGS);
	bs.Write(settings.port);
	bs.Write(settings.codec);
	bs.Write(settings.useHardwareEncoder);
	m_Peer->Send(&bs, PacketPriority::IMMEDIATE_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_RAKNET_GUID, true);
}

void CCTVServer::Disconnect(RakNetGUID client) {
	//this "kicks" a client from our server:
	for (auto& addr : m_Clients) {
		if (addr.second == client) {
			m_Peer->CloseConnection(addr.second, true);
		}
	}
}

RGBAImage CCTVServer::GenerateRandomRGBAImage(int width, int height) {
	frameCount++;

	//Create the new image:
	RGBAImage toReturn;
	toReturn.size = width * height * 4;
	//toReturn.data.resize(toReturn.size);
	toReturn.data = new unsigned char[toReturn.size];

	//Generate our image:
	int pos = 0;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			pos = (i * width + j) * 4;
			toReturn.data[pos] = sin(frameCount) * 255; // red
			toReturn.data[pos + 1] = cos(frameCount) * 255; // green
			toReturn.data[pos + 2] = tan(frameCount) * 255; // blue
			toReturn.data[pos + 3] = 255; // alpha
		}
	}

	return toReturn;
}
