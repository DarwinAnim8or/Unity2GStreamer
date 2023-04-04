// MediaLAN 02/2013
// CRtspSession
// - Main and streamer master listener socket
// - Client session thread

#include "stdafx.h"

#include <thread>
#include <future>
#include <vector>
#include <fstream>

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
#include "RakSleep.h"

//Custom includes:
#include "NetworkDefines.h"
#include "StreamDataManager.h"
#include "LockFreeQueue.h"

using namespace RakNet;

LockFreeQueue frameQueue(2, 1920000);
LockFreeQueue frameQueue2(2, 1920000);
LockFreeQueue frameQueue3(2, 1920000);
//char* data = new char[2289828];

StreamDataManager sm();
bool shouldTransmitRTSP = false;
unsigned short rtspPort = 8554;

unsigned int size = 0;
unsigned int width, height;

bool run = true;

//If we're on Win32, we'll include our minidump writer
#ifdef WIN32
#include <Windows.h>
#include <Dbghelp.h>

void make_minidump(EXCEPTION_POINTERS* e) {
    auto hDbgHelp = LoadLibraryA("dbghelp");
    if (hDbgHelp == nullptr)
        return;
    auto pMiniDumpWriteDump = (decltype(&MiniDumpWriteDump))GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (pMiniDumpWriteDump == nullptr)
        return;

    char name[MAX_PATH];
    {
        auto nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);
        SYSTEMTIME t;
        GetSystemTime(&t);
        wsprintfA(nameEnd - strlen(".exe"),
            "_%4d%02d%02d_%02d%02d%02d.dmp",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    }

    auto hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = e;
    exceptionInfo.ClientPointers = FALSE;

    auto dumped = pMiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
        e ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);

    CloseHandle(hFile);

    return;
}

LONG CALLBACK unhandled_handler(EXCEPTION_POINTERS* e) {
    make_minidump(e);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

DWORD WINAPI SessionThreadHandler(LPVOID lpParam)
{
    SOCKET Client = *(SOCKET*)lpParam;
    char* data = new char[2289828];

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
    double T = 66.66;                                       // frame rate in MS (25fps == 40.0, 15 == 66.66)
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
                    //if (!dataMutex.try_lock()) continue;

                    int size;
                    StreamID = RtspSession.GetStreamID();
                    if (StreamID == 0) {
                        frameQueue.pop(data, size);
                    } else if (StreamID == 1) {
                        frameQueue2.pop(data, size);
                    } else if (StreamID == 2) {
                        frameQueue3.pop(data, size);
                    }
                    
                    //frameQueue.pop(data, size);

                    Streamer.StreamImage(data, size, width, height);

                    //dataMutex.unlock();
                }
                break;
            };
        };
    };

    if (HTimer) CloseHandle(HTimer);
    closesocket(Client);
    return 0;
};

void RakNetLoop() {
    //If we're Win32, we'll set this so our exe will write a minidump when it crashes.
#ifdef WIN32
    SetUnhandledExceptionFilter(unhandled_handler);
#endif

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

    bool skip = false;

    while (run) {
        for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) {
            //Check for packets from RakNet :
            //packet = peer->Receive();
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
                    //StreamDataManager::Instance().AddStream(1);
                    shouldTransmitRTSP = true;
                    rtspPort = (unsigned short)sSettings.port;

                    break;
                }

                case (int)Messages::ID_IMAGE_DATA:
                {
                    //skip = !skip;
                    //if (skip) continue;

                    RakNet::BitStream is(packet->data, packet->length, false);
                    MessageID msgID;
                    StreamData newData;
                    is.Read(msgID);
                    newData.Deserialize(is);

                    //Insert our image data into our lock-free queue:
                    //frameQueue.push((char*)newData.data, newData.size);

                    if (newData.channelID == 1) {
                        frameQueue.push((char*)newData.data, newData.size);
                    } else if (newData.channelID == 2) {
                        frameQueue2.push((char*)newData.data, newData.size);
                    } else if (newData.channelID == 3) {
                        frameQueue3.push((char*)newData.data, newData.size);
                    }

                    //temp write out frame data:
                    std::ofstream file("frame.bin");
                    for (int i = 0; i < newData.size; i++)
                        file << newData.data[i];
                    file.close();

                    width = newData.width;
                    height = newData.height;

                    /*if (g_dataMutex[newData.channelID].try_lock()) {
                        if (data[newData.channelID]) delete[] data[newData.channelID];

                        if (size != newData.size)
                            data[newData.channelID] = new char[newData.size];

                        data[newData.channelID] = (char*)newData.data;
                        size = newData.size;

                        g_dataMutex[newData.channelID].unlock();
                    }*/

                    /*if (dataMutex.try_lock()) {
                        if (data) delete[] data;

                        if (size != newData.size)
                            data = new char[newData.size];

                        data = (char*)newData.data;
                        size = newData.size;

                        dataMutex.unlock();
                    }*/

                    break;
                }

                case (int)Messages::ID_CREATE_NEW_CHANNEL:
                {
                    std::string cmd = "start ./EncodingClient.exe " + std::to_string(rtspPort + 1);
                    system(cmd.c_str());
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

            //peer->DeallocatePacket(packet);
        }

        RakSleep(10); //RakNet thread does sleep to prevent high CPU usage, but it will run faster than RTSP can stream
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

    if (argc == 2)
        ServerAddr.sin_port = htons((unsigned short)std::stoi(argv[1]));                 // listen on RTSP port 8554
    else
        ServerAddr.sin_port = htons(8554);

    MasterSocket               = socket(AF_INET,SOCK_STREAM,0);   

    bool hasSetupRTSP = false;

    std::thread t{ RakNetLoop };

    while (run) {
        if (!shouldTransmitRTSP) continue;
        else if (!hasSetupRTSP) {
            // bind our master socket to the RTSP port and listen for a client connection
            if (bind(MasterSocket, (sockaddr*)&ServerAddr, sizeof(ServerAddr)) != 0) return 0;
            if (listen(MasterSocket, 5) != 0) return 0;

            hasSetupRTSP = true; //make sure we don't set it up a second time
        }

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