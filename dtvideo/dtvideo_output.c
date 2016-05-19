#include "dtvideo_output.h"
#include "dtvideo_decoder.h"
#include "dtvideo.h"

#define TAG "VIDEO-OUT"

#define REGISTER_VO(X, x)       \
    {                           \
        extern vo_wrapper_t vo_##x##_ops; \
        register_vo(&vo_##x##_ops); \
    }

static vo_wrapper_t *g_vo = NULL;

static void register_vo(vo_wrapper_t * vo)
{
    vo_wrapper_t **p;
    p = &g_vo;
    while (*p != NULL) {
        p = &((*p)->next);
    }
    *p = vo;
    vo->next = NULL;
    dt_info(TAG, "register vo. id:%d name:%s \n", vo->id, vo->name);
}

void vout_register_ext(vo_wrapper_t * vo)
{
    vo_wrapper_t **p;
    p = &g_vo;
    if (*p == NULL) {
        *p = vo;
        vo->next = NULL;
    } else {
        vo->next = *p;
        *p = vo;
    }

    dt_info(TAG, "register ext vo. id:%d name:%s \n", vo->id, vo->name);
}

void vout_register_all()
{
    /*Register all audio_output */
    REGISTER_VO(NULL, null);

#if 0
#ifdef ENABLE_VO_SDL
    REGISTER_VO(SDL, sdl);
#endif
#ifdef ENABLE_VO_SDL2
    REGISTER_VO(SDL2, sdl2);
#endif
#endif

    return;
}

void vout_remove_all()
{
    g_vo = NULL;
}

/*default alsa*/
int select_vo_device(dtvideo_output_t * vo, int id)
{
    vo_wrapper_t **p;
    p = &g_vo;

    if (id == -1) { // user did not choose vo,use default one
        if (!*p) {
            return -1;
        }
        vo->wrapper = *p;
        dt_info(TAG, "SELECT VO:%s \n", (*p)->name);
        return 0;
    }

    while (*p != NULL && (*p)->id != id) {
        p = &(*p)->next;
    }
    if (!*p) {
        dt_error(TAG, "no valid vo device found\n");
        return -1;
    }
    vo->wrapper = *p;
    dt_info(TAG, "SELECT VO:%s \n", (*p)->name);
    return 0;
}

int video_output_start(dtvideo_output_t * vo)
{
    /*start playback */
    vo->status = VO_STATUS_RUNNING;
    return 0;
}

int video_output_pause(dtvideo_output_t * vo)
{
    vo->status = VO_STATUS_PAUSE;
    return 0;
}

int video_output_resume(dtvideo_output_t * vo)
{
    vo->status = VO_STATUS_RUNNING;
    return 0;
}

int video_output_stop(dtvideo_output_t * vo)
{
    vo_wrapper_t *wrapper = vo->wrapper;
    vo->status = VO_STATUS_EXIT;
    pthread_join(vo->output_thread_pid, NULL);
    wrapper->vo_stop(vo);
    dt_info(TAG, "[%s:%d] vout stop ok \n", __FUNCTION__, __LINE__);
    return 0;
}

int video_output_latency(dtvideo_output_t * vo)
{
    return 0;
#if 0
    if (ao->status == AO_STATUS_IDLE) {
        return 0;
    }
    if (ao->status == AO_STATUS_PAUSE) {
        return ao->last_valid_latency;
    }
    ao->last_valid_latency = ao->aout_ops->ao_latency(ao);
    return ao->last_valid_latency;
#endif
}

int video_output_get_level(dtvideo_output_t * ao)
{
    return 0;
    //return ao->state.aout_buf_level;
}

static void dump_frame(dt_av_frame_t * pFrame, int index)
{
    FILE *pFile;
    char szFilename[32];
    int y;
    int width = pFrame->width;
    int height = pFrame->height;
    sprintf(szFilename, "frame%d.ppm", index); // setup filename
    pFile = fopen(szFilename, "wb");           // open file
    if (pFile == NULL) {
        return;
    }
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);  // write header
    // write pixel data
#if 1
    for (y = 0; y < height; y++) {
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);
    }
#endif
#if 0
    int i, j, shift;
    char *yuv = NULL;
    for (i = 0; i < 3; i++) {
        shift = (i == 0 ? 0 : 1);
        yuv = pFrame->data[i];
        for (j = 0; j < height >> shift; j++) {
            fwrite(yuv, (width >> shift), 1, pFile);
            yuv_factor += pFrame->linesize[i];
        }
    }
#endif
#if 0
    fwrite(pFrame->data[0], 1, pFrame->linesize[0] * height, pFile);
    fwrite(pFrame->data[1], 1, pFrame->linesize[1] * height / 2, pFile);
    fwrite(pFrame->data[2], 1, pFrame->linesize[2] * height / 3, pFile);
#endif
    fclose(pFile);  // close
}

//output one frame to output gragh
//using pts
#define REFRESH_DURATION 10*1000 //us
static void *video_output_thread(void *args)
{
    dtvideo_output_t *vo = (dtvideo_output_t *) args;
    dtvideo_context_t *vctx = (dtvideo_context_t *) vo->parent;
    dtvideo_filter_t *filter = (dtvideo_filter_t *) & (vctx->video_filt);
    vo_wrapper_t *wrapper = vo->wrapper;
    int ret, wlen;
    ret = wlen = 0;
    dt_av_frame_t *picture_pre;
    dt_av_frame_t *picture;
    dt_av_frame_t *pic;

    int64_t render_clock_start = -1;
    int render_mode = dtp_setting.video_render_mode;
    int render_ms = dtp_setting.video_render_duration;

    // sys_clock_*** use to calc system time
    int64_t sys_clock;
    int64_t video_discontinue = 0;

    int dump_mode = dtp_setting.player_dump_mode; // 5 for video dump
    int dump_index = 0;
    for (;;) {
        if (vo->status == VO_STATUS_EXIT) {
            goto EXIT;
        }
        if (vo->status == VO_STATUS_IDLE || vo->status == VO_STATUS_PAUSE) {
            usleep(100);
            continue;
        }
        /*pre read picture and update sys time */
        picture_pre = (dt_av_frame_t *) dtvideo_output_pre_read(vo->parent);
        if (!picture_pre) {
            dt_debug(TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep(100);
            continue;
        }

        /* render mode == VIDEO_RENDER_MODE_DURATION */
        if (render_mode == VIDEO_RENDER_MODE_DURATION) {
            if (render_clock_start == -1) {
                render_clock_start = dt_gettime();
            } else if ((llabs)(dt_gettime() - render_clock_start) / 1000 > render_ms) {
                render_clock_start = dt_gettime();
            } else {
                continue;
            }
            picture = (dt_av_frame_t *) dtvideo_output_read(vo->parent);
            pic = (dt_av_frame_t *) picture;
            goto RENDER;
        }

        sys_clock = dtvideo_get_systime(vo->parent);

        /* vpts invalid, if audio exists, Host will set clock using apts */
        if (PTS_INVALID(sys_clock)) {
            dt_info(TAG, "SETTING FIRST SYSCLOK:%llx \n", picture_pre->pts);
            sys_clock = picture_pre->pts;
            dtvideo_update_systime(vo->parent, sys_clock);
        }

        //update sys time
        dtvideo_update_systime(vo->parent, sys_clock);

        // check video discontinue
        if (vctx->last_valid_pts != -1 && picture_pre->pts != -1) {
            int64_t step = llabs(picture_pre->pts - vctx->last_valid_pts);
            if (step >= DT_SYNC_DISCONTINUE_THRESHOLD) {
                video_discontinue = 1;
                dt_info(TAG, "video discontinue occured, step:%lld(%d ms) \n", step, step / DT_PTS_FREQ_MS);
            }
        }

        if (video_discontinue == 0) { // if discontinue, display directly
            //maybe need to block
            if (sys_clock < picture_pre->pts) {
                dt_debug(TAG, "[%s:%d] not to show ! pts:%lld systime:%lld  \n", __FUNCTION__, __LINE__, picture_pre->pts, sys_clock);
                dt_usleep(REFRESH_DURATION);
                continue;
            }
        }
        /*read data from filter or decode buffer */
        picture = (dt_av_frame_t *) dtvideo_output_read(vo->parent);
        if (!picture) {
            dt_error(TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep(1000);
            continue;
        }
        pic = (dt_av_frame_t *) picture;
        //update pts
        if (vctx->last_valid_pts == -1) {
            vctx->last_valid_pts = vctx->current_pts = picture->pts;
        } else {
            vctx->last_valid_pts = vctx->current_pts;
            vctx->current_pts = picture->pts;
            if (video_discontinue) {
                vctx->last_valid_pts = vctx->current_pts;
            }
        }
        if (video_discontinue == 0) {
            /*read next frame ,check drop frame */
            picture_pre = (dt_av_frame_t *) dtvideo_output_pre_read(vo->parent);
            if (picture_pre) {
                if (picture_pre->pts == -1) {
                    dt_debug(TAG, "can not get vpts from frame,estimate using fps:%d  \n", vo->para->fps);
                    picture_pre->pts = vctx->current_pts + 90000 / vo->para->fps;
                }

                if (sys_clock >= picture_pre->pts) {
                    dt_info(TAG, "drop frame,sys clock:%lld thispts:%lld next->pts:%lld \n", sys_clock, picture->pts, picture_pre->pts);
                    dtvideo_update_pts(vo->parent);  // drop means not render, but need update vpts
                    dtav_clear_frame(pic);
                    free(picture);
                    continue;
                }
            }
        }
RENDER:
        /*display picture & update vpts */
        ret = wrapper->vo_render(vo, pic);
        if (ret < 0) {
            dt_error(TAG, "frame toggle failed! \n");
            usleep(1000);
        } else {
            // dump video check
            if (dump_mode == 5 && dump_index < 5) {
                dump_frame(pic, dump_index++);
            }
        }

        /*update vpts */
        dtvideo_update_pts(vo->parent);
        dtav_clear_frame(pic);
        free(picture);
        video_discontinue = 0;
        //dt_usleep (REFRESH_DURATION);
    }
EXIT:
    dt_info(TAG, "[file:%s][%s:%d]ao playback thread exit\n", __FILE__, __FUNCTION__, __LINE__);
    pthread_exit(NULL);
    return NULL;
}

int video_output_init(dtvideo_output_t * vo, int vo_id)
{
    int ret = 0;
    pthread_t tid;

    /*select ao device */
    ret = select_vo_device(vo, vo_id);
    if (ret < 0) {
        return -1;
    }
    vo_wrapper_t *wrapper = vo->wrapper;
    wrapper->vo_init(vo);
    dt_info(TAG, "[%s:%d] video output init success\n", __FUNCTION__, __LINE__);

    /*start aout pthread */
    ret = pthread_create(&tid, NULL, video_output_thread, (void *) vo);
    if (ret != 0) {
        dt_error(TAG, "[%s:%d] create video output thread failed\n", __FUNCTION__, __LINE__);
        return ret;
    }
    vo->output_thread_pid = tid;
    dt_info(TAG, "[%s:%d] create video output thread success\n", __FUNCTION__, __LINE__);
    return 0;
}

uint64_t video_output_get_latency(dtvideo_output_t * vo)
{
    return 0;
}
