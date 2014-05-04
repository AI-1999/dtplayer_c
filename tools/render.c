#include "render.h"

#include "dtaudio.h"
#include "dtaudio_output.h"
#include "dtvideo.h"
#include "dtvideo_output.h"

/*---------------------------------------------------------- */
#ifdef ENABLE_VO_SDL2
extern vo_wrapper_t vo_sdl2_ops;
#endif

vo_wrapper_t vo_ex_ops;

static int vo_ex_init ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = vo_sdl2_ops.vo_init();  
#endif
    return ret; 
}

static int vo_ex_render (AVPicture_t * pict)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2
    ret = vo_sdl2_ops.vo_render(pict);
#endif
    return ret;
}

static int vo_ex_stop ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2
    ret = vo_sdl2_ops.vo_stop();
#endif
    return ret;
}

vo_wrapper_t vo_ex_ops = {
    .id = VO_ID_EX,
    .name = "ex vo",
    .vo_init = vo_ex_init,
    .vo_stop = vo_ex_stop,
    .vo_render = vo_ex_render,
};



/*---------------------------------------------------------- */
#ifdef ENABLE_VO_SDL2 
extern ao_wrapper_t ao_sdl2_ops;
#endif

ao_wrapper_t ao_ex_ops;
ao_wrapper_t *ao_wrapper = &ao_ex_ops;
static int ao_ex_init (dtaudio_para_t *para)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_init(para);
#endif
    return ret;
}

static int ao_ex_play (uint8_t * buf, int size)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_write(buf,size);
#endif
    return ret;
}

static int ao_ex_pause ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_pause();
#endif
    return ret;
}

static int ao_ex_resume ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_resume();
#endif
    return ret;
}

static int ao_ex_level()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_level();
#endif
    return ret;
}

static int64_t ao_ex_get_latency ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_latency();
#endif
    return ret;
}

static int ao_ex_stop ()
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_stop();
#endif
    return ret;
}

ao_wrapper_t ao_ex_ops = {
    .id = AO_ID_EX,
    .name = "ex ao",
    .ao_init = ao_ex_init,
    .ao_pause = ao_ex_pause,
    .ao_resume = ao_ex_resume,
    .ao_stop = ao_ex_stop,
    .ao_write = ao_ex_play,
    .ao_level = ao_ex_level,
    .ao_latency = ao_ex_get_latency,
};


int render_init()
{
    register_ext_ao(&ao_ex_ops);
    register_ext_vo(&vo_ex_ops); 

    return  0;
}

int render_stop()
{
    return 0;
}


