#pragma once

#include <gst/gst.h>
#include <stdint.h>
#include <gst/gstelement.h>

static GstFlowReturn on_new_sample(GstElement* pipeline, gpointer user_data)
{
    return GST_FLOW_OK;
}

void start_rtsp_stream(unsigned char* pixelData, int width, int height);