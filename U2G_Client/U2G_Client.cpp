#include <iostream>
#include <string>

//RakNet includes:
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"

//IMPORTANT: This should stay in sync with the client project. Increment the netversion if you make changes to this enum or the serialization of packets.
int NET_VERSION = 2;

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

//our gstreamer pipeline
#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstbus.h>

//Define our variables:
GstElement* pipeline;
GstElement* appsrc;
GstElement* x264enc;
GstElement* rtph264pay;
GstElement* udpsink;

GstCaps* caps;
GstBus* bus;

void InitializeGStreamer(StreamSettings settings) {
	// Initialize GStreamer:
	if (!gst_init_check(NULL, NULL, NULL)) {
		std::cout << "Failed to initialize GStreamer." << std::endl;
		return;
	}

	// Create our pipeline:
	GstElement* pipeline = gst_pipeline_new("rtsp-pipeline");
	GstElement* src = gst_element_factory_make("fakesrc", NULL);
	if (src == nullptr) std::cout << "null";
	GstElement* appsrc = gst_element_factory_make("appsrc", "appsrc");

	// Configure our newly created source:
	GstCaps* caps = gst_caps_new_simple("video/x-raw",
		"format", G_TYPE_STRING, "RGBA",
		"width", G_TYPE_INT, 1920,
		"height", G_TYPE_INT, 1080,
		"framerate", GST_TYPE_FRACTION, 15, 1,
		NULL);

	g_object_set(G_OBJECT(appsrc), "caps", caps, "stream-type", 0, NULL);
	gst_app_src_set_max_bytes(GST_APP_SRC(appsrc), 0);

	// Create an H.264 encoder element:
	GstElement* x264enc = gst_element_factory_make("x264enc", "x264enc");
	GstElement* rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");
	GstElement* udpsink = gst_element_factory_make("udpsink", "udpsink");

	g_object_set(G_OBJECT(udpsink), "host", "127.0.0.1", "port", settings.port, NULL);

	// Add our elements to the pipeline:
	gst_bin_add_many(GST_BIN(pipeline), appsrc, x264enc, rtph264pay, udpsink, NULL);

	// Link our elements:
	if (!gst_element_link_many(appsrc, x264enc, rtph264pay, udpsink, NULL)) {
		std::cout << "Failed to link GStreamer elements." << std::endl;
		gst_object_unref(pipeline);
		return;
	}

	// Set the pipeline to playing:
	GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		std::cout << "Failed to start GStreamer pipeline." << std::endl;
		gst_object_unref(pipeline);
		return;
	}

	std::cout << "GStreamer pipeline initialized." << std::endl;
	std::cout << "RTSP stream running on port " << settings.port << std::endl;
}

void PushPixelDataToGStream(unsigned char* data, unsigned int size) {
	GstBuffer* buffer = gst_buffer_new_allocate(NULL, size, NULL);
	gst_buffer_fill(buffer, 0, data, size);
	gst_app_src_push_buffer((GstAppSrc*)appsrc, buffer);

	std::cout << "Pushed " << size << " bytes to GStreamer." << std::endl;
}

void ShutdownGStreamer() {
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
}

int main(void) {
	RakPeerInterface* peer = RakPeerInterface::GetInstance();
	Packet* packet;
	SocketDescriptor socket;
	peer->Startup(1, &socket, 1);

	//Connect to the server:
	peer->Connect("127.0.0.1", 3001, "U2GPass", 7); //TODO: make ip & port read from a configuration file. 

	//To store the server's details:
	RakNetGUID serverGUID;

	StreamSettings sSettings;

	//Our main networking loop
	bool run = true;
	while (run) {
		for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) {
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
				
				InitializeGStreamer(sSettings);

				break;
			}

			case (int)Messages::ID_IMAGE_DATA: {
				if (pipeline == NULL) break;
				RakNet::BitStream is(packet->data, packet->length, false);
				MessageID msgID;
				unsigned int size;
				unsigned char* data;

				is.Read(msgID);
				is.Read(size);
				data = new unsigned char[size];
				
				for (unsigned int i = 0; i < size; i++) {
					unsigned char temp;
					is.Read(temp);
					if (temp != 0x00) std::cout << "not 0" << std::endl;
					data[i] = temp;
				}

				PushPixelDataToGStream(data, size);

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
