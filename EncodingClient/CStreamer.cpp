// MediaLAN 02/2013
// CRtspSession
// - JPEG packetizer and UDP/TCP based streaming

#include "CStreamer.h"

#include <stdio.h>
#include <fstream>
#include <string>
#include "math.h"
#include "turbojpeg.h"

#include "StreamDataManager.h"
#include "JpegFrameParser.h"

#include "snappy-c.h"

CStreamer::CStreamer(SOCKET aClient, bool useTCP = false) :
    m_Client(aClient),
    m_TCPTransport(useTCP)
{
    m_RtpServerPort  = 0;
    m_RtcpServerPort = 0;
    m_RtpClientPort  = 0;
    m_RtcpClientPort = 0;

    m_SequenceNumber = 0;
    m_AudioSequenceNumber = 0;
    m_Timestamp      = 0;
    m_AudioTimestamp = 0;
    m_SendIdx        = 0;
};

CStreamer::~CStreamer()
{
    closesocket(m_RtpSocket);
    closesocket(m_RtcpSocket);
};

// See JpegFrameParser.cpp, Copyright (C) 2011, W.L. Chuang <ponponli2000 at gmail.com>
// to see how to parse a jpeg file for tables, scan data, ..
// RFC 2435 by Berc, et al. has C code at the end to show how to send jpeg frames

void CStreamer::SendRtpJpegPacket(const unsigned char * Jpeg, int JpegLen, int width, int height, const unsigned char * Tables, int TablesLen, int offset, bool End, int Chn)
{
    char        RtpBuf[4096];
    sockaddr_in RecvAddr;
    int         RecvLen = sizeof(RecvAddr);
    int         RtpPacketSize;

    if (offset == 0) {
      RtpPacketSize = 12 + 8 + 4 + TablesLen + JpegLen; // RTSP Header + (4 + TablesLen) for Tables + (8 + Jpeglen) for Jpeg body
    } else {
      RtpPacketSize = 12 + 8 + JpegLen; // RTSP Header + (8 + Jpeglen) for Jpeg body
    }

    // get client address for UDP transport
    getpeername(m_Client,(struct sockaddr*)&RecvAddr,&RecvLen);
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port   = htons(m_RtpClientPort);

    memset(RtpBuf,0x00,sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0]  = '$';        // magic number
    RtpBuf[1]  = 0;          // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2]  = (char) ((RtpPacketSize & 0x0000FF00) >> 8);
    RtpBuf[3]  = (char) ((RtpPacketSize & 0x000000FF));
    // Prepare the 12 byte RTP header
    RtpBuf[4]  = (char) 0x80;                               // RTP version
    RtpBuf[5]  = (char) 0x1A;                               // JPEG payload (26) and marker bit
    RtpBuf[7]  = (char) (m_SequenceNumber & 0x0FF);           // each packet is counted with a sequence counter
    RtpBuf[6]  = (char) (m_SequenceNumber >> 8);
    RtpBuf[8]  = (char) ((m_Timestamp & 0xFF000000) >> 24);   // each image gets a timestamp
    RtpBuf[9]  = (char) ((m_Timestamp & 0x00FF0000) >> 16);
    RtpBuf[10] = (char) ((m_Timestamp & 0x0000FF00) >> 8);
    RtpBuf[11] = (char) (m_Timestamp & 0x000000FF);
    RtpBuf[12] = (char) 0x13;                               // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = (char) 0xf9;                               // we just an arbitrary number here to keep it simple
    RtpBuf[14] = (char) 0x7e;
    RtpBuf[15] = (char) 0x67;

    // Prepare the 8 byte payload JPEG header
    RtpBuf[16] = (char) 0x00;                        // type specific
    RtpBuf[17] = (char) ((offset & 0x00FF0000) >> 16);        // 3 byte fragmentation offset for fragmented images
    RtpBuf[18] = (char) ((offset & 0x0000FF00) >> 8);
    RtpBuf[19] = (char) (offset & 0x000000FF);
    RtpBuf[20] = (char) 0x01;                        // type
    RtpBuf[21] = (char) 0xFF;                        // quality scale factor
    RtpBuf[22] = (char) width;                       // width  / 8
    RtpBuf[23] = (char) height;                      // height / 8

    // No restart interval

    if (offset == 0) {
      // Table data
      RtpBuf[24] = 0; // MBZ
      RtpBuf[25] = 0; // precision
      RtpBuf[26] = (char) ((TablesLen & 0x0000FF00) >> 8);
      RtpBuf[27]  = (char) ((TablesLen & 0x000000FF));
    
      // Tables
      memcpy(&RtpBuf[28], Tables, TablesLen);
      // append the JPEG scan data to the RTP buffer
      memcpy(&RtpBuf[28 + TablesLen],Jpeg,JpegLen);
    } else {
      // append the JPEG scan data to the RTP buffer
      memcpy(&RtpBuf[24],Jpeg,JpegLen);
    }

    if (End) { // mark the last fragment
      RtpBuf[5] |= (char) 0x80;
      m_Timestamp += 3600;                             // fixed timestamp increment for a frame rate of 25fps
    }
    m_SequenceNumber++;                              // prepare the packet counter for the next packet

    if (m_TCPTransport) // RTP over RTSP - we send the buffer + 4 byte additional header
        send(m_Client,RtpBuf,RtpPacketSize + 4,0);
    else                // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
        sendto(m_RtpSocket,&RtpBuf[4],RtpPacketSize,0,(SOCKADDR *) & RecvAddr,sizeof(RecvAddr));
};

void CStreamer::SendRtpPacket(char * Jpeg, int JpegLen, int Chn)
{
#define KRtpHeaderSize 12           // size of the RTP header
#define KJpegHeaderSize 8           // size of the special JPEG payload header

    char        RtpBuf[4096];
    sockaddr_in RecvAddr;
    int         RecvLen = sizeof(RecvAddr);
    int         RtpPacketSize = JpegLen + KRtpHeaderSize + KJpegHeaderSize;

    // get client address for UDP transport
    getpeername(m_Client,(struct sockaddr*)&RecvAddr,&RecvLen);
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port   = htons(m_RtpClientPort);

    memset(RtpBuf,0x00,sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0]  = '$';        // magic number
    RtpBuf[1]  = 0;          // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2]  = (char) ((RtpPacketSize & 0x0000FF00) >> 8);
    RtpBuf[3]  = (char) ((RtpPacketSize & 0x000000FF));
    // Prepare the 12 byte RTP header
    RtpBuf[4]  = (char) 0x80;                        // RTP version
    RtpBuf[5]  = (char) 0x9a;                        // JPEG payload (26) and marker bit
    RtpBuf[7]  = (char) (m_SequenceNumber & 0x0FF);  // each packet is counted with a sequence counter
    RtpBuf[6]  = (char) (m_SequenceNumber >> 8);
    RtpBuf[8]  = (char) ((m_Timestamp & 0xFF000000) >> 24);   // each image gets a timestamp
    RtpBuf[9]  = (char) ((m_Timestamp & 0x00FF0000) >> 16);
    RtpBuf[10] = (char) ((m_Timestamp & 0x0000FF00) >> 8);
    RtpBuf[11] = (char) ((m_Timestamp & 0x000000FF));
    RtpBuf[12] = (char) 0x13;                               // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = (char) 0xf9;                               // we just an arbitrary number here to keep it simple
    RtpBuf[14] = (char) 0x7e;
    RtpBuf[15] = (char) 0x67;

    // Prepare the 8 byte payload JPEG header
    RtpBuf[16] = (char) 0x00;                               // type specific
    RtpBuf[17] = (char) 0x00;                               // 3 byte fragmentation offset for fragmented images
    RtpBuf[18] = (char) 0x00;
    RtpBuf[19] = (char) 0x00;
    RtpBuf[20] = (char) 0x01;                               // type
    RtpBuf[21] = (char) 0x5e;                               // quality scale factor
    if (Chn == 0)
    {
        RtpBuf[22] = (char) 0x06;                           // width  / 8 -> 48 pixel
        RtpBuf[23] = (char) 0x04;                           // height / 8 -> 32 pixel
    }
    else
    {
        RtpBuf[22] = (char) 0x08;                           // width  / 8 -> 64 pixel
        RtpBuf[23] = (char) 0x06;                           // height / 8 -> 48 pixel
    };
    // append the JPEG scan data to the RTP buffer
    memcpy(&RtpBuf[24],Jpeg,JpegLen);
    
    m_SequenceNumber++;                              // prepare the packet counter for the next packet
    m_Timestamp += 3600;                             // fixed timestamp increment for a frame rate of 25fps

    if (m_TCPTransport) // RTP over RTSP - we send the buffer + 4 byte additional header
        send(m_Client,RtpBuf,RtpPacketSize + 4,0);
    else                // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
        sendto(m_RtpSocket,&RtpBuf[4],RtpPacketSize,0,(SOCKADDR *) & RecvAddr,sizeof(RecvAddr));
};

void CStreamer::SendRtpAudioPacket(char * Audio, int AudioLen, int Chn)
{
#define KRtpHeaderSize 12           // size of the RTP header

    char        RtpBuf[4096];
    sockaddr_in RecvAddr;
    int         RecvLen = sizeof(RecvAddr);
    int         RtpPacketSize = AudioLen + KRtpHeaderSize;

    // get client address for UDP transport
    getpeername(m_Client,(struct sockaddr*)&RecvAddr,&RecvLen);
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port   = htons(m_RtpAudioClientPort);

    memset(RtpBuf,0x00,sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0]  = '$';        // magic number
    RtpBuf[1]  = 0;          // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2]  = (RtpPacketSize & 0x0000FF00) >> 8;
    RtpBuf[3]  = (RtpPacketSize & 0x000000FF);
    // Prepare the 12 byte RTP header
    RtpBuf[4]  = (char) 0x80;                               // RTP version
    RtpBuf[5]  = (char) 0x08;
    RtpBuf[7]  = (char) (m_AudioSequenceNumber & 0x0FF);           // each packet is counted with a sequence counter
    RtpBuf[6]  = (char) (m_AudioSequenceNumber >> 8);
    RtpBuf[8]  = (char) ((m_AudioTimestamp & 0xFF000000) >> 24);   // each image gets a timestamp
    RtpBuf[9]  = (char) ((m_AudioTimestamp & 0x00FF0000) >> 16);
    RtpBuf[10] = (char) ((m_AudioTimestamp & 0x0000FF00) >> 8);
    RtpBuf[11] = (char) ((m_AudioTimestamp & 0x000000FF));
    RtpBuf[12] = (char) 0x20;                               // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = (char) 0xf9;                               // we just an arbitrary number here to keep it simple
    RtpBuf[14] = (char) 0x7e;
    RtpBuf[15] = (char) 0x67;

    // append the JPEG scan data to the RTP buffer
    memcpy(&RtpBuf[16],Audio,AudioLen);
    
    m_AudioSequenceNumber++;                              // prepare the packet counter for the next packet
    m_AudioTimestamp += 320;                             // fixed timestamp increment for a frame rate of 25fps

    if (m_TCPTransport) // RTP over RTSP - we send the buffer + 4 byte additional header
        send(m_Client,RtpBuf,RtpPacketSize + 4,0);
    else {                // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
        sendto(m_RtpAudioSocket,&RtpBuf[4],RtpPacketSize,0,(SOCKADDR *) & RecvAddr,sizeof(RecvAddr));
    }
};

void CStreamer::InitTransport(u_short aRtpPort, u_short aRtcpPort, bool TCP, bool video)
{
    sockaddr_in Server;
    sockaddr_in ServerAudio;

    if (video) {
      m_RtpClientPort  = aRtpPort;
      m_RtcpClientPort = aRtcpPort;
      m_TCPTransport   = TCP;

      printf("RTP Client Port: %d RTCP Client Port: %d\n\n", aRtpPort, aRtcpPort);
      if (!m_TCPTransport)
      {   // allocate port pairs for RTP/RTCP ports in UDP transport mode
        Server.sin_family      = AF_INET;   
        Server.sin_addr.s_addr = INADDR_ANY;   
        for (u_short P = 6970; P < 0xFFFE ; P += 2)
        {
          m_RtpSocket     = socket(AF_INET, SOCK_DGRAM, 0);                     
          Server.sin_port = htons(P);
          if (bind(m_RtpSocket,(sockaddr*)&Server,sizeof(Server)) == 0)
          {   // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            m_RtcpSocket = socket(AF_INET, SOCK_DGRAM, 0);
            Server.sin_port = htons(P + 1);
            if (bind(m_RtcpSocket,(sockaddr*)&Server,sizeof(Server)) == 0) 
            {
              m_RtpServerPort  = P;
              m_RtcpServerPort = P+1;
              break; 
            }
            else
            {
              printf("Cannot bind %d - %d\n", P, P+1);
              closesocket(m_RtpSocket);
              closesocket(m_RtcpSocket);
            };
          }
          else {
            printf("Cannot bind %d\n", P);
            closesocket(m_RtpSocket);
          }
        };
      };
    } else {
      m_RtpAudioClientPort  = aRtpPort;
      m_RtcpAudioClientPort = aRtcpPort;
      m_TCPTransport   = TCP;

      printf("RTP Audio Client Port: %d RTCP Audio Client Port: %d\n\n", aRtpPort, aRtcpPort);
      if (!m_TCPTransport)
      {   // allocate port pairs for RTP/RTCP ports in UDP transport mode
        ServerAudio.sin_family      = AF_INET;   
        ServerAudio.sin_addr.s_addr = INADDR_ANY;   
        for (u_short P = 6970; P < 0xFFFE ; P += 2)
        {
          m_RtpAudioSocket     = socket(AF_INET, SOCK_DGRAM, 0);                     
          ServerAudio.sin_port = htons(P);
          if (bind(m_RtpAudioSocket,(sockaddr*)&ServerAudio,sizeof(ServerAudio)) == 0)
          {   // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            m_RtcpAudioSocket = socket(AF_INET, SOCK_DGRAM, 0);
            ServerAudio.sin_port = htons(P + 1);
            if (bind(m_RtcpAudioSocket,(sockaddr*)&ServerAudio,sizeof(ServerAudio)) == 0) 
            {
              m_RtpAudioServerPort  = P;
              m_RtcpAudioServerPort = P+1;
              break; 
            }
            else
            {
              printf("Cannot bind %d - %d\n", P, P+1);
              closesocket(m_RtpAudioSocket);
              closesocket(m_RtcpAudioSocket);
            };
          }
          else {
            printf("Cannot bind %d\n", P);
            closesocket(m_RtpAudioSocket);
          }
        };
      };
    }
};

u_short CStreamer::GetRtpServerPort()
{
    return m_RtpServerPort;
};

u_short CStreamer::GetRtcpServerPort()
{
    return m_RtcpServerPort;
};

u_short CStreamer::GetRtpAudioServerPort()
{
    return m_RtpAudioServerPort;
};

u_short CStreamer::GetRtcpAudioServerPort()
{
    return m_RtcpAudioServerPort;
};

struct RGBAImage {
    unsigned char* data = nullptr;
    unsigned int size = 0;

    ~RGBAImage() {
        //delete[] data;
    }
};

static int frameCount = 0;

RGBAImage GenerateRandomRGBAImage(int width, int height) {
    frameCount++;

    //Create the new image:
    RGBAImage toReturn;
    toReturn.size = width * height * 4;
    toReturn.data = new unsigned char[toReturn.size];

    //Generate our image:
    int pos = 0;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            pos = (i * width + j) * 4;

            //toReturn.data[pos] = sin(frameCount) * 255; // red
            //toReturn.data[pos + 1] = cos(frameCount) * 255; // green
            //toReturn.data[pos + 2] = tan(frameCount) * 255; // blue
            //toReturn.data[pos + 3] = 255; // alpha

            //simple effect:
            float t = (float)frameCount * 4.0f;
            int vv = int(
                (127.0f + (127.0f * sinf(j / 7.0f + t))) +
                (127.0f + (127.0f * sinf(i / 5.0f - t))) +
                (127.0f + (127.0f * sinf((j + i) / 6.0f - t))) +
                (127.0f + (127.0f * sinf(sqrtf(float(j * j + i * i)) / 4.0f - t)))
                ) / 4;

            toReturn.data[pos] = vv;
            toReturn.data[pos + 1] = vv;
            toReturn.data[pos + 2] = vv;
            toReturn.data[pos + 3] = vv;
        }
    }

    return toReturn;
}

int framec = 0;
std::ofstream file("timings.csv");
//file << "Frame,Time taken in ms" << std::endl;

void LogToCSV(double time) {
    framec++;
    file << framec << "," << time << std::endl;
}

#define PACKETSIZE 1400
void CStreamer::StreamImage(char* data, unsigned int length, unsigned int width, unsigned int height) {
    try {
        auto t1 = std::chrono::high_resolution_clock::now();

        //We need to decompress the image using snappy first:
        size_t uncompressedLength = width * height * 4;
        //snappy_status status = snappy_uncompressed_length(data, length, &uncompressedLength);
        char* uncompressedImage = new char[uncompressedLength];

        snappy_uncompress(data, length, uncompressedImage, &uncompressedLength);

        // Use libjpeg-turbo to encode this as a JPEG image:
        tjhandle jpegCompressor = tjInitCompress();

        // Set JPEG quality as a constant or configuration parameter
        const int jpegQuality = 75;

        //Because we keep the compressed image as a member, keep track of the largest size so we don't allocate a new one each frame
        m_compressedImageSize = m_allocatedImageSize;

        m_compressedImageSize = length;
        m_compressedImage = (unsigned char*)data;

        // Compress image using libjpeg-turbo
        int tjResult = tjCompress2(jpegCompressor, (const unsigned char*)uncompressedImage, width, 0, height, TJPF_ARGB, &m_compressedImage, &m_compressedImageSize, TJSAMP_420, jpegQuality, TJFLAG_FASTDCT);

        // Check for compression errors
        if (tjResult != 0) {
            printf("Failed to compress image: %s\n", tjGetErrorStr());
            tjDestroy(jpegCompressor);
            return;
        }

        //if the last size is greater than our stored size, update it:
        m_allocatedImageSize = m_allocatedImageSize >= m_compressedImageSize ? m_allocatedImageSize : m_compressedImageSize;

        // Send compressed image over RTSP

        //For testing, write the jpeg to disk:
        /*std::ofstream outFile("./test.jpg", std::ios_base::binary);
        if (outFile) {
            for (unsigned long i = 0; i < m_compressedImageSize; i++) {
                outFile << m_compressedImage[i];
            }
        }

        outFile.close();*/

        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        //LogToCSV(duration);

        //Parse the generated jpeg:
        JpegFrameParser parser;
        parser.parse(m_compressedImage, m_compressedImageSize);

        //Set our header and scan data:
        unsigned short tablesLength;
        auto tables = parser.quantizationTables(tablesLength);

        unsigned int scanLength;
        auto scanData = parser.scandata(scanLength);

        //Finally send the data:
        bool isLastPacket = false;
        unsigned int length, sentLength;
        sentLength = 0;

        //while we haven't sent all our jpeg scan data yet:
        while (sentLength < scanLength) {
            //if the sum of sent data and bytes per packet is less than the total scan length, we'll be sending up a follow-up packet containing more image data.
            if (sentLength + PACKETSIZE < scanLength) {
                length = PACKETSIZE;
                isLastPacket = false;
            }
            else { //this is triggered when we're sending our last bits of data
                length = scanLength - sentLength;
                isLastPacket = true;
            }

            SendRtpJpegPacket(scanData + sentLength, length, width / 8, height / 8, tables, tablesLength, sentLength, isLastPacket, 1);
            sentLength += length;
        }

        // Free memory allocated by libjpeg-turbo
        //tjFree(m_compressedImage);
        tjDestroy(jpegCompressor);

        //Free memory by parser:
        //delete[] parser.scandata;
        delete[] uncompressedImage;
    }
    catch (const std::exception& e) {
        // Handle exceptions
        printf("Exception caught: %s\n", e.what());
    }
};

// http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
unsigned char ALaw_Encode(short int number)
{
   const unsigned short int ALAW_MAX = 0xFFF;
   unsigned short int mask = 0x800;
   unsigned short int sign = 0;
   unsigned short int position = 11;
   unsigned char lsb = 0;
   if (number < 0)
   {
      number = -number;
      sign = 0x80;
   }
   if (number > ALAW_MAX)
   {
      number = ALAW_MAX;
   }
   for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--);
   lsb = (number >> ((position == 4) ? (1) : (position - 4))) & 0x0f;
   return (sign | ((position - 4) << 4) | lsb) ^ 0x55;
}

int AudioCount = 0;
void CStreamer::StreamAudio(int StreamID)
{
  char audio[320];
  int i, len = 320;
  double pi = 3.14159;

  for (i = 0; i < len; i++) {
    audio[i] = ALaw_Encode((short) (32767.0 * sin(2.0 * pi * (double) (AudioCount++) * (1000.0 + StreamID * 500.0) / 8000.0)));
  }
  SendRtpAudioPacket(audio, len, StreamID);
}

