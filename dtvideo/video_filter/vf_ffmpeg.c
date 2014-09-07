#include "vf_wrapper.h"
#include "dtvideo_pic.h"

#include "libavcodec/avcodec.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"


#define TAG "VF-FFMPEG"

typedef struct vf_ffmpeg_ctx
{
    struct SwsContext *pSwsCtx;;
    int need_process_flag;

}vf_ffmpeg_ctx_t;

static int ffmpeg_vf_capable(dtvideo_filter_t *filter)
{
    return VF_CAP_CONVERT | VF_CAP_CROP; 
}

static int need_process(dtvideo_para_t *para)
{
    int sw = para->s_width;
    int sh = para->s_height;
    int dw = para->d_width;
    int dh = para->d_height;
    int sf = para->s_pixfmt;
    int df = para->d_pixfmt;

    return !(sw == dw && sh == dh && sf == df);

}

static int ffmpeg_vf_init(dtvideo_filter_t *filter)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)malloc(sizeof(vf_ffmpeg_ctx_t));
    if(!vf_ctx)
        return -1;
    memset(vf_ctx,0,sizeof(*vf_ctx));
    dtvideo_para_t *para = &filter->para;
    vf_ctx->need_process_flag = need_process(para);
    filter->vf_priv = vf_ctx;
    dt_info (TAG, "[%s:%d] vf init ok ,need process:%d \n", __FUNCTION__, __LINE__,vf_ctx->need_process_flag);
    return 0;
}

static int convert_picture (dtvideo_filter_t * filter, dt_av_pic_t * src)
{
    uint8_t *buffer;
    int buffer_size;
    
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    dtvideo_para_t *para = &filter->para;
    int sw = para->s_width; 
    int dw = para->d_width; 
    int sh = para->s_height; 
    int dh = para->d_height; 
    int sf = para->s_pixfmt; 
    int df = para->d_pixfmt; 

    dt_info (TAG, "[%s:%d] sw:%d dw:%d sh:%d dh:%d sf:%d df:%d \n", __FUNCTION__, __LINE__,sw,dw,sh,dh,sf,df);
    const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get(sf);
    //step1: malloc avpicture_t
    dt_av_pic_t *pict = dtav_new_pic();
    if(!pict)
        return -1;
    memset (pict, 0, sizeof (dt_av_pic_t));
    AVPicture *dst = (AVPicture *)pict;
    //step3: allocate an AVFrame structure
    buffer_size = avpicture_get_size (df, dw, dh);
    buffer = (uint8_t *) malloc (buffer_size * sizeof (uint8_t));
    avpicture_fill ((AVPicture *) dst, buffer, df, dw, dh);
    dt_info(TAG, "start filter \n");

    //re setup linesize
    src->linesize[0] = av_image_get_linesize(sf, sw, 0);
    src->linesize[1] = av_image_get_linesize(sf, sw, 1);
    src->linesize[2] = av_image_get_linesize(sf, sw, 2);

    src->data[0] = src->data[0];
    src->data[1] = src->data[0] + src->linesize[0] * sh;
    src->data[2] = src->data[1] + src->linesize[1] * -(-sh>>pix_desc->log2_chroma_h);

    vf_ctx->pSwsCtx = sws_getCachedContext (vf_ctx->pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    dt_info(TAG, "get cached context \n");
    sws_scale (vf_ctx->pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);
    dt_info(TAG, "filter end \n");
    
    pict->pts = src->pts;
    dtav_unref_pic(src);
    memcpy(src,pict,sizeof(dt_av_pic_t));

    return 0;
}

static int ffmpeg_vf_process(dtvideo_filter_t *filter, dt_av_pic_t *pic)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    if(!vf_ctx->need_process_flag) 
        return 0;
    
    int ret = convert_picture(filter,pic);
    if(ret < 0)
    {
        dt_info (TAG, "[%s:%d] vf process failed \n", __FUNCTION__, __LINE__);
        return -1;
    } 
    return 0;
}

static int ffmpeg_vf_release(dtvideo_filter_t *filter)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    if(!vf_ctx)
        return 0;
    if (vf_ctx->pSwsCtx)
        sws_freeContext (vf_ctx->pSwsCtx);
    free(vf_ctx);
    dt_info (TAG, "[%s:%d] vf release ok \n", __FUNCTION__, __LINE__);
    return 0;
}

vf_wrapper_t vf_ffmpeg_ops = {
    .name       = "ffmpeg video filter",
    .type       = DT_TYPE_VIDEO,
    .capable    = ffmpeg_vf_capable,
    .init       = ffmpeg_vf_init,
    .process    = ffmpeg_vf_process,
    .release    = ffmpeg_vf_release,
};