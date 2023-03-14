// MediaLAN 02/2013
// CRtspSession
// - Main and streamer master listener socket
// - Client session thread

#include "stdafx.h"

#include <thread>
#include <future>

#include <Winsock2.h>
#include <windows.h>  
#pragma comment(lib, "Ws2_32.lib")

#include "CStreamer.h"
#include "CRtspSession.h"

//RakNet includes:
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"

//Custom includes:
#include "NetworkDefines.h"
#include "StreamDataManager.h"

using namespace RakNet;

StreamDataManager sm();
bool shouldTransmitRTSP = false;

DWORD WINAPI SessionThreadHandler(LPVOID lpParam)
{
    SOCKET Client = *(SOCKET*)lpParam;

    char         RecvBuf[10000];                    // receiver buffer
    int          res;  
    CStreamer    Streamer(Client, true);                  // our streamer for UDP/TCP based RTP transport
    CRtspSession RtspSession(Client,&Streamer);     // our threads RTSP session and state
    int          StreamID = 0;                      // the ID of the 2 JPEG samples streams which we support
    HANDLE       WaitEvents[2];                     // the waitable kernel objects of our session
    
    HANDLE HTimer = ::CreateWaitableTimerA(NULL,false, NULL);

    WSAEVENT RtspReadEvent = WSACreateEvent();      // create READ wait event for our RTSP client socket
    WSAEventSelect(Client,RtspReadEvent,FD_READ);   // select socket read event
    WaitEvents[0] = RtspReadEvent;
    WaitEvents[1] = HTimer;

    // set frame rate timer
    double T = 40.0;                                       // frame rate
    int iT   = (int) T;
    const __int64 DueTime = - static_cast<const __int64>(iT) * 10 * 1000;
    ::SetWaitableTimer(HTimer,reinterpret_cast<const LARGE_INTEGER*>(&DueTime),iT,NULL,NULL,false);

    bool StreamingStarted = false;
    bool Stop             = false;

    while (!Stop)
    {
        switch (WaitForMultipleObjects(2,WaitEvents,false,INFINITE))
        {
            case WAIT_OBJECT_0 + 0 : 
            {   // read client socket
                WSAResetEvent(WaitEvents[0]);

                memset(RecvBuf,0x00,sizeof(RecvBuf));
                res = recv(Client,RecvBuf,sizeof(RecvBuf),0);

                //printf("Server:\n%s\n\n", RecvBuf);

                // we filter away everything which seems not to be an RTSP command: O-ption, D-escribe, S-etup, P-lay, T-eardown
                if ((RecvBuf[0] == 'O') || (RecvBuf[0] == 'D') || (RecvBuf[0] == 'S') || (RecvBuf[0] == 'P') || (RecvBuf[0] == 'T'))
                {
                    RTSP_CMD_TYPES C = RtspSession.Handle_RtspRequest(RecvBuf,res);
                    if (C == RTSP_PLAY)     StreamingStarted = true; else if (C == RTSP_TEARDOWN) Stop = true;
                };
                break;      
            };
            case WAIT_OBJECT_0 + 1 : 
            {
                if (StreamingStarted) {
                  Streamer.StreamImage(RtspSession.GetStreamID());
                  //Streamer.StreamAudio(RtspSession.GetStreamID());
                }
                break;
            };
        };
    };
    closesocket(Client);
    return 0;
};

void RakNetLoop() {
    //RakNet variables:
    RakPeerInterface* peer = RakPeerInterface::GetInstance();
    Packet* packet;
    SocketDescriptor socket;
    peer->Startup(1, &socket, 1);

    //Set up our defaults:
    std::string ip = "127.0.0.1";
    int port = 3001;

    std::cout << "Opening connection to: " << ip << ":" << port << std::endl;

    //Connect to the server:
    peer->Connect(ip.c_str(), port, "U2GPass", 7); //TODO: make ip & port read from a configuration file as well.

    //To store the server's details:
    RakNetGUID serverGUID;
    StreamSettings sSettings;

    bool run = true;
    while (run) {
        //Check for packets from RakNet :
        packet = peer->Receive();
        if (packet) {
            switch ((MessageID)packet->data[0]) {
            case ID_DISCONNECTION_NOTIFICATION:
            case ID_CONNECTION_LOST:
                std::cout << "Disconnected." << std::endl;
                run = false;
                break;

            case ID_CONNECTION_REQUEST_ACCEPTED:
            {
                std::cout << "Our connection request has been accepted. Sending handshake..." << std::endl;
                serverGUID = packet->guid;
                RakNet::BitStream bs;
                bs.Write((MessageID)Messages::ID_HANDSHAKE);
                bs.Write<int>(NET_VERSION);
                peer->Send(&bs, PacketPriority::HIGH_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0, serverGUID, false);
                break;
            }

            case (int)Messages::ID_HANDSHAKE_RESPONSE:
            {
                RakNet::BitStream is(packet->data, packet->length, false);
                MessageID msgID;
                bool isOk = false;
                is.Read(msgID);
                is.Read(isOk);

                std::cout << "Handshake accepted: " << (int)isOk << std::endl;
                if (!isOk) run = false;
                else std::cout << "Awaiting streaming settings." << std::endl;
                break;
            }

            case (int)Messages::ID_STREAM_SETTINGS:
            {
                RakNet::BitStream is(packet->data, packet->length, false);
                MessageID msgID;
                is.Read(msgID);
                is.Read(sSettings.port);
                is.Read(sSettings.codec);
                is.Read(sSettings.useHardwareEncoder);

                std::cout << "Stream settings received. Port: " << sSettings.port << ", Codec: " << sSettings.codec.C_String() << ", Use hardware encoder: " << sSettings.useHardwareEncoder << std::endl;

                //TODO: change our settings based on what we just received
                StreamDataManager::Instance().AddStream(1);
                shouldTransmitRTSP = true;

                break;
            }

            case (int)Messages::ID_IMAGE_DATA:
            {
                std::cout << "img data wee" << std::endl;
                RakNet::BitStream is(packet->data, packet->length, false);
                MessageID msgID;
                unsigned int width;
                unsigned int height;
                unsigned int size;
                unsigned char* data;

                is.Read(msgID);
                is.Read(size);
                data = new unsigned char[size];

                for (unsigned int i = 0; i < size; i++) {
                    unsigned char temp;
                    is.Read(temp);
                    data[i] = temp;
                }

                StreamData newData;
                newData.data = data;
                newData.size = size;

                StreamDataManager::Instance().SetDataForStream(1, newData);

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

        peer->DeallocatePacket(packet);
    }

    RakPeerInterface::DestroyInstance(peer);
}

int main(int argc, char** argv) {    
    SOCKET      MasterSocket;                                 // our masterSocket(socket that listens for RTSP client connections)  
    SOCKET      ClientSocket;                                 // RTSP socket to handle an client
    sockaddr_in ServerAddr;                                   // server address parameters
    sockaddr_in ClientAddr;                                   // address parameters of a new RTSP client
    int         ClientAddrLen = sizeof(ClientAddr);   
    WSADATA     WsaData;  
    DWORD       TID;

    int ret = WSAStartup(0x101,&WsaData); 
    if (ret != 0) return 0;  

    ServerAddr.sin_family      = AF_INET;   
    ServerAddr.sin_addr.s_addr = INADDR_ANY;   
    ServerAddr.sin_port        = htons(8554);                 // listen on RTSP port 8554
    MasterSocket               = socket(AF_INET,SOCK_STREAM,0);   

    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(MasterSocket,(sockaddr*)&ServerAddr,sizeof(ServerAddr)) != 0) return 0;  
    if (listen(MasterSocket,5) != 0) return 0;

    std::thread t{ RakNetLoop };

    bool run = true;

    while (run) {
        if (!shouldTransmitRTSP) continue;

        ClientSocket = accept(MasterSocket, (struct sockaddr*)&ClientAddr, &ClientAddrLen);
        CreateThread(NULL, 0, SessionThreadHandler, &ClientSocket, 0, &TID);
        printf("Client connected. Client address: %s\r\n", inet_ntoa(ClientAddr.sin_addr));
    }  

    //Clean up:
    t.join(); //wait for the other thread to exit gracefully
    StreamDataManager::Instance().Cleanup();

    closesocket(MasterSocket);   
    WSACleanup();   

    return 0;  
}