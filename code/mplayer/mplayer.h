/* date = November 18th 2024 4:00 pm */

#ifndef MPLAYER_H
#define MPLAYER_H


#include "mplayer_input.h"
struct Mplayer_Context;
struct Render_Context;

struct Sound_Config
{
	u32 sample_rate;
	u16 channels_count;
};


#define MPLAYER_INITIALIZE(name) Mplayer_Context *name(Memory_Arena *frame_arena, Render_Context *render_ctx, Mplayer_Input *input, OS_Vtable *vtable)
#define MPLAYER_HOTLOAD(name) void name(Mplayer_Context *_mplayer, Mplayer_Input *input, OS_Vtable *vtable)
#define MPLAYER_UPDATE_AND_RENDER(name) void name()
#define MPLAYER_GET_AUDIO_SAMPLES(name) void name(Sound_Config device_config, void *output_buf, u32 frame_count)

typedef MPLAYER_INITIALIZE(Mplayer_Initialize_Proc);
typedef MPLAYER_HOTLOAD(Mplayer_Hotload_Proc);
typedef MPLAYER_UPDATE_AND_RENDER(Mplayer_Update_And_Render_Proc);
typedef MPLAYER_GET_AUDIO_SAMPLES(Mplayer_Get_Audio_Samples_Proc);

struct Mplayer_Exported_Vtable
{
	Mplayer_Initialize_Proc *mplayer_initialize;
	Mplayer_Hotload_Proc    *mplayer_hotload;
	Mplayer_Update_And_Render_Proc *mplayer_update_and_render;
	Mplayer_Get_Audio_Samples_Proc *mplayer_get_audio_samples;
};


#define MPLAYER_EXPORT(name) Mplayer_Exported_Vtable name()
typedef MPLAYER_EXPORT(Mplayer_Get_Vtable);
exported MPLAYER_EXPORT(mplayer_get_vtable);


MPLAYER_INITIALIZE(mplayer_initialize_stub) {return 0;}
MPLAYER_HOTLOAD(mplayer_hotload_stub) {}
MPLAYER_UPDATE_AND_RENDER(mplayer_update_and_render_stub) {}
MPLAYER_GET_AUDIO_SAMPLES(mplayer_get_audio_samples_stub) {}

global Mplayer_Exported_Vtable g_vtable_stub = {
	.mplayer_initialize = mplayer_initialize_stub,
	.mplayer_hotload = mplayer_hotload_stub,
	.mplayer_update_and_render = mplayer_update_and_render_stub,
	.mplayer_get_audio_samples = mplayer_get_audio_samples_stub,
};


#endif //MPLAYER_H
