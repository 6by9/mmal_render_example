/*
 * MMAL Video render example app
 *
 * Copyright © 2017 Raspberry Pi (Trading) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>


#define ENCODING    MMAL_ENCODING_RGB24
#define WIDTH  128
#define HEIGHT 128

#define MAX_ENCODINGS_NUM 25
typedef struct {
   MMAL_PARAMETER_HEADER_T header;
   MMAL_FOURCC_T encodings[MAX_ENCODINGS_NUM];
} MMAL_SUPPORTED_ENCODINGS_T;


int print_supported_formats(MMAL_PORT_T *port)
{
   MMAL_STATUS_T ret;

   MMAL_SUPPORTED_ENCODINGS_T sup_encodings = {{MMAL_PARAMETER_SUPPORTED_ENCODINGS, sizeof(sup_encodings)}, {0}};
   ret = mmal_port_parameter_get(port, &sup_encodings.header);
   if (ret == MMAL_SUCCESS || ret == MMAL_ENOSPC)
   {
      //Allow ENOSPC error and hope that the desired formats are in the first
      //MAX_ENCODINGS_NUM entries.
      int i;
      int num_encodings = (sup_encodings.header.size - sizeof(sup_encodings.header)) /
          sizeof(sup_encodings.encodings[0]);
      if(num_encodings > MAX_ENCODINGS_NUM)
         num_encodings = MAX_ENCODINGS_NUM;
      for (i=0; i<num_encodings; i++)
      {
         printf("%u: %4.4s\n", i, (char*)&sup_encodings.encodings[i]);
      }
   }
   return 0;
}

static void callback_vr_input(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_buffer_header_release(buffer);
}

int main()
{
    MMAL_COMPONENT_T *render = NULL;
    MMAL_PORT_T *input;
    MMAL_POOL_T *pool;
    MMAL_BUFFER_HEADER_T *buffer;
    int i;

    mmal_component_create("vc.ril.video_render", &render);
    input = render->input[0];

    print_supported_formats(input);

    input->format->encoding = ENCODING;
    input->format->es->video.width  = VCOS_ALIGN_UP(WIDTH,  32);
    input->format->es->video.height = VCOS_ALIGN_UP(HEIGHT, 16);
    input->format->es->video.crop.x = 0;
    input->format->es->video.crop.y = 0;
    input->format->es->video.crop.width  = WIDTH;
    input->format->es->video.crop.height = HEIGHT;
    mmal_port_format_commit(input);

    mmal_component_enable(render);

    mmal_port_parameter_set_boolean(input, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);

    input->buffer_size = input->buffer_size_recommended;
    input->buffer_num = input->buffer_num_recommended;
    if (input->buffer_num < 2)
        input->buffer_num = 2;
    pool = mmal_port_pool_create(input, input->buffer_num, input->buffer_size);

    if (!pool) {
        printf("Oops, ,pool alloc failed\n");
        return -1;
    }

    {
        MMAL_DISPLAYREGION_T param;
        param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

        param.set = MMAL_DISPLAY_SET_LAYER;
        param.layer = 128;    //On top of most things

        param.set |= MMAL_DISPLAY_SET_ALPHA;
        param.alpha = 255;    //0 = transparent, 255 = opaque

        param.set |= (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
        param.fullscreen = 0;
        param.dest_rect.x = 100;
        param.dest_rect.y = 200;
        param.dest_rect.width = WIDTH;
        param.dest_rect.height = HEIGHT;
        mmal_port_parameter_set(input, &param.hdr);
    }

    mmal_port_enable(input, callback_vr_input);

    for (i=0; i<10; i++) {
        buffer = mmal_queue_wait(pool->queue);

        // Write something into the buffer.
        memset(buffer->data, (i<<4)&0xff, buffer->alloc_size);

        buffer->length = buffer->alloc_size;
        mmal_port_send_buffer(input, buffer);

        sleep(1);
    }

    mmal_port_disable(input);
    mmal_component_destroy(render);

    return 0;
}
