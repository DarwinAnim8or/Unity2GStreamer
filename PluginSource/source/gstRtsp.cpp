#include "gstRtsp.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstelement.h>
#include <gst/gstbus.h>

void start_rtsp_stream(unsigned char* pixelData, int width, int height) {
    //Define our variables:
    GstElement* pipeline;
    GstElement* appsrc;
    GstElement* x264enc;
    GstElement* rtph264pay;
    GstElement* udpsink;

    GstCaps* caps;
    GstBus* bus;

    //initialize gstreamer:
    gst_init(NULL, NULL);

    pipeline = gst_pipeline_new("rtsp-pipeline");
    appsrc = gst_element_factory_make("appsrc", "appsrc");

    //configure our newly created source:
    caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    g_object_set(G_OBJECT(appsrc), "caps", caps, "stream-type", 0, NULL);
    gst_app_src_set_max_bytes((GstAppSrc*)appsrc, 0);

    //create a h264 encoder element (not cuda for now):
    x264enc = gst_element_factory_make("x264enc", "x264enc");
    rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");
    udpsink = gst_element_factory_make("udpsink", "udpsink");

    g_object_set(G_OBJECT(udpsink), "host", "127.0.0.1", "port", 8554, NULL);

    //add elements to the scene
    gst_bin_add_many(GST_BIN(pipeline), appsrc, x264enc, rtph264pay, udpsink, NULL);

    //link them 
    gst_element_link_many(appsrc, x264enc, rtph264pay, udpsink, NULL);

    //set the state of the stream:
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set pipeline to playing state\n");
        gst_object_unref(pipeline);
        return;
    }

    //push pixel data to the stream
    GstBuffer* buffer = gst_buffer_new_wrapped(pixelData, width * height * 4);
    gst_app_src_push_buffer((GstAppSrc*)appsrc, buffer);

    //listen on our bus for messages:
    bus = gst_element_get_bus(pipeline);
    GstMessage* msg;
    while (msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR))) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            break;
        default:
            //ginore
            break;
        }

        gst_message_unref(msg);
    }

    //Free resources:
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}