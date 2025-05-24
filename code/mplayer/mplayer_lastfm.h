/* date = April 19th 2025 11:59 am */

#ifndef MPLAYER_LASTFM_H
#define MPLAYER_LASTFM_H

struct Mplayer_Lastfm
{
	Memory_Arena arena;
	b32 valid;
	String8 username;
	String8 session_key;
	
	// NOTE(fakhri): used during authentication, no longer is valid after end_authentication call
	String8 token;
};

#endif //MPLAYER_LASTFM_H
