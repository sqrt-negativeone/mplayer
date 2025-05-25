#include "third_party/discord_rpc.h"
#define DISCORD_CLIENT_ID "1375870442778394705"

internal void
mplayer_discord_init()
{
	DiscordEventHandlers handlers = ZERO_STRUCT;
	Discord_Initialize(DISCORD_CLIENT_ID, &handlers, 1, NULL);
}

internal void
mplayer_discord_update_rich_presence()
{
	DiscordRichPresence presence = ZERO_STRUCT;
	Mplayer_Track *current_track = mplayer_queue_get_current_track();
	char details[128];
	char state[128];
	if (current_track)
	{
		if (mplayer_ctx->queue.playing)
		{
			stbsp_snprintf(details, sizeof(details), "🎶  %.*s", STR8_EXPAND(current_track->title));
			
			presence.startTimestamp = current_track->start_ts;
			presence.endTimestamp   = presence.startTimestamp + mplayer_get_track_duration(current_track);
		}
		else
		{
			stbsp_snprintf(details, sizeof(details), "⏸  %.*s (Paused)", STR8_EXPAND(current_track->title));
		}
		
		stbsp_snprintf(state, sizeof(state), "by %.*s | %.*s", STR8_EXPAND(current_track->album), STR8_EXPAND(current_track->artist));
		
		presence.details  = details;
		presence.state    = state;
		
		if (!current_track->image_url.len && !current_track->did_try_to_load_image_url)
		{
			current_track->did_try_to_load_image_url = true;
			platform->push_work(mplayer_get_track_image_url__async,  current_track);
		}
		
		if (current_track->image_url.len)
		{
			char large_iamge_txt[128];
			stbsp_snprintf(large_iamge_txt, sizeof(large_iamge_txt), "%.*s", STR8_EXPAND(current_track->image_url));
			presence.largeImageKey = large_iamge_txt;
		}
	}
	else
	{
		presence.details  = "🎧 Music Player";
		presence.state    = "Ready to play music";
	}
	
	presence.type = ActivityType_Listening;
	presence.name = "Mplayer";
	Discord_UpdatePresence(&presence);
	Discord_RunCallbacks();
}