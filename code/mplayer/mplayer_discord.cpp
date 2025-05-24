
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
		}
		else
		{
			stbsp_snprintf(details, sizeof(details), "⏸  %.*s", STR8_EXPAND(current_track->title));
		}
		
		stbsp_snprintf(state, sizeof(state), "by %.*s | %.*s", STR8_EXPAND(current_track->album), STR8_EXPAND(current_track->artist));
		presence.startTimestamp = current_track->start_ts;
		
		presence.details  = details;
		presence.state    = state;
	}
	else
	{
		presence.details  = "🎧 Music Player";
		presence.state    = "Ready to play music";
	}
	Discord_UpdatePresence(&presence);
	Discord_RunCallbacks();
}