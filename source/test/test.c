/*
 * test.c
 *
 * Description of this file:
 *    test the AVS2 Video Decoder ���� davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
 *    Copyright (C) 2018~ VCL, NELVT, Peking University
 *
 *    Authors: Falei LUO <falei.luo@gmail.com>
 *             etc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 *    This program is also available under a commercial proprietary license.
 *    For more information, contact us at sswang @ pku.edu.cn.
 */

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#if HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include "davs2.h"
#include "psnr.h"
#include "utils.h"
#include "parse_args.h"
#include "inputstream.h"

#if defined(_MSC_VER)
#pragma comment(lib, "libdavs2.lib")
#endif

#if defined(__cplusplus)
extern "C" {
#endif  /* __cplusplus */

/**
 * ===========================================================================
 * macro defines
 * ===========================================================================
 */
#define CTRL_LOOP_DEC_FILE    0   /* ѭ������һ��ES�ļ� */

/* ---------------------------------------------------------------------------
 * disable warning C4100: : unreferenced formal parameter
 */
#ifndef UNREFERENCED_PARAMETER
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define UNREFERENCED_PARAMETER(v) (v)
#else
#define UNREFERENCED_PARAMETER(v) (void)(v)
#endif
#endif


/**
 * ===========================================================================
 * global variables
 * ===========================================================================
 */
int g_frmcount = 0;
int g_psnrfail = 0;

davs2_input_param_t inputparam = {
    NULL, NULL, NULL, 0, 0, 0
};


/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static 
void DumpFrames(davs2_picture_t *pic, davs2_seq_info_t *headerset, int num_frames)
{
    static char IMGTYPE[] = {'I', 'P', 'B', 'G', 'F', 'S', '\x0'};
    double psnr_y = 0.0f, psnr_u = 0.0f, psnr_v = 0.0f;

    if (headerset == NULL) {
        return;
    }

    if (pic == NULL || pic->ret_type == DAVS2_GOT_HEADER) {
        show_message(CONSOLE_GREEN,
            "  Sequence size: %dx%d; BitDepth: %d/%d, FrameRate: %.3lf Hz\n\n", 
            headerset->horizontal_size, headerset->vertical_size, 
            headerset->internal_bitdepth, headerset->output_bitdepth,
            headerset->frame_rate);
        return;
    }

    if (inputparam.g_psnr) {
        int ret = cal_psnr(pic->pic_order_count, pic->planes, inputparam.g_recfile,
                           pic->width[0], pic->lines[0], pic->i_pic_planes,
                           &psnr_y, &psnr_u, &psnr_v, 
                           pic->bytes_per_sample, pic->pic_bit_depth);
        int psnr = (psnr_y != 0 || psnr_u != 0 || psnr_v != 0);

        if (ret < 0) {
            g_psnrfail = 1;
            show_message(CONSOLE_RED, "failed to cal psnr for frame %d(%d).\t\t\t\t\n", g_frmcount, pic->pic_order_count);
        } else {
            if (inputparam.g_verbose || psnr) {
                show_message(psnr ? CONSOLE_RED : CONSOLE_WHITE,
                    "%5d(%d)\t(%c) %3d\t%8.4lf %8.4lf %8.4lf\t\t\n", 
                    g_frmcount, pic->pic_order_count,
                    IMGTYPE[pic->type], pic->QP, psnr_y, psnr_u, psnr_v);
            }
        }
    } else if (inputparam.g_verbose) {
        show_message(CONSOLE_WHITE,
            "%5d(%d)\t(%c)\t%3d\n", g_frmcount, pic->pic_order_count, IMGTYPE[pic->type], pic->QP);
    }

    g_frmcount++;

    if (inputparam.g_verbose == 0) {
        show_progress(g_frmcount, num_frames);
    }

    if (inputparam.g_outfile) {
        write_frame(pic, inputparam.g_outfile);
    }
}

/* ---------------------------------------------------------------------------
 * data_buf - pointer to bitstream buffer
 * data_len - number of bytes in bitstream buffer
 * frames   - number of frames in bitstream buffer
 */
void test_decoder(uint8_t *data_buf, int data_len, int num_frames, char *dst)
{
    const double f_time_fac = 1.0 / (double)CLOCKS_PER_SEC;
    davs2_param_t    param;      // decoding parameters
    davs2_packet_t   packet;     // input bitstream
    davs2_picture_t  out_frame;  // output data, frame data
    davs2_seq_info_t headerset;  // output data, sequence header
    int ret;

#if CTRL_LOOP_DEC_FILE
    uint8_t *bak_data_buf = data_buf;
    int      bak_data_len = data_len;
    int      num_loop     = 5;      // ѭ���������
#endif
    int64_t time0, time1;
    void *decoder;

    /* init the decoder */
    param.threads      = inputparam.g_threads;
    param.opaque       = (void *)(intptr_t)num_frames;
    param.i_info_level = DAVS2_LOG_DEBUG;

    decoder = davs2_decoder_open(&param);

    time0 = get_time();

    /* do decoding */
    for (;;) {
        int len = 128;
        if (data_len < len) {
            len           = data_len;
            packet.marker = 1;
        } else {
            packet.marker = 0;
        }

        packet.data = data_buf;
        packet.len  = len;
        packet.pts  = 0;        /* clear the user pts */
        packet.dts  = 0;        /* clear the user dts */

        len = davs2_decoder_decode(decoder, &packet, &headerset, &out_frame);           //����ʹ�õ���������

        if (out_frame.ret_type != DAVS2_DEFAULT) {
            DumpFrames(&out_frame, &headerset, num_frames);
            davs2_decoder_frame_unref(decoder, &out_frame);
        }

        if (len < 0) {
            printf("Error: An decoder error counted\n");
            break;
        }

        data_buf += len;
        data_len -= len;

        if (data_len == 0) {
#if CTRL_LOOP_DEC_FILE
            data_buf = bak_data_buf;
            data_len = bak_data_len;
            num_loop--;
            if (num_loop <= 0) {
                break;
            }
#else
            break;              /* end of bitstream */
#endif
        }
    }

    /* flush the decoder */
    for (;;) {
        ret = davs2_decoder_flush(decoder, &headerset, &out_frame);
        if (ret < 0) {
            break;
        }
        if (out_frame.ret_type != DAVS2_DEFAULT) {
            DumpFrames(&out_frame, &headerset, num_frames);
            davs2_decoder_frame_unref(decoder, &out_frame);
        }
    }

    time1 = get_time();

    /* close the decoder */
    davs2_decoder_close(decoder);

    /* statistics */
    show_message(CONSOLE_WHITE, "\n--------------------------------------------------\n");

    show_message(CONSOLE_GREEN, "total fames: %d/%d\n", g_frmcount, num_frames);
    if (inputparam.g_psnr) {
        if (g_psnrfail == 0 && g_frmcount != 0) {
            show_message(CONSOLE_GREEN,
                         "average PSNR:\t%8.4f, %8.4f, %8.4f\n\n", 
                         g_sum_psnr_y / g_frmcount, g_sum_psnr_u / g_frmcount, g_sum_psnr_v / g_frmcount);

            sprintf(dst, "  Frames: %d/%d\n  TIME : %.3lfs, %6.2lf fps\n  PSNR : %8.4f, %8.4f, %8.4f\n",
                    g_frmcount, num_frames,
                    (double)((time1 - time0) * f_time_fac),
                    (double)(g_frmcount / ((time1 - time0) * f_time_fac)),
                    g_sum_psnr_y / g_frmcount, g_sum_psnr_u / g_frmcount, g_sum_psnr_v / g_frmcount);
        } else {
            show_message(CONSOLE_RED, "average PSNR:\tNaN, \tNaN, \tNaN\n\n"); /* 'NaN' for 'Not a Number' */
        }
    }

    show_message(CONSOLE_GREEN, "total decoding time: %.3lfs, %6.2lf fps\n", 
        (double)((time1 - time0) * f_time_fac), 
        (double)(g_frmcount / ((time1 - time0) * f_time_fac)));
}

/* ---------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
    char dst[1024] = "> no decode data\n";
    uint8_t *data = NULL;
    clock_t tm_start = clock();
    int size;
    int frames;

    /* parse params */
    if (parse_args(&inputparam, argc, argv) < 0) {
        sprintf(dst, "Failed to parse input parameters\n");
        goto fail;
    }

    /* read input data */
    if (read_input_file(&inputparam, &data, &size, &frames, 0.0f) < 0) {
        sprintf(dst, "Failed to read input bit-stream or create output file\n");
        goto fail;
    }

    /* test decoding */
    test_decoder(data, size, frames, dst);

    printf("\n Decoder Total Time: %.3lf s\n", (clock() - tm_start) / (double)(CLOCKS_PER_SEC));

fail:
    /* tidy up */
    if (data) {
        free(data);
    }

    if (g_recbuf) {
        free(g_recbuf);
    }

    if (inputparam.g_infile) {
        fclose(inputparam.g_infile);
    }

    if (inputparam.g_recfile) {
        fclose(inputparam.g_recfile);
    }

    if (inputparam.g_outfile) {
        fclose(inputparam.g_outfile);
    }

    printf(" Decoder Exit, Time: %.3lf s\n", (clock() - tm_start) / (double)(CLOCKS_PER_SEC));
    return 0;
}

#if defined(__cplusplus)
}
#endif  /* __cplusplus */
