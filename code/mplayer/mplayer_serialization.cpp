
internal void
mplayer_serialize(File_Handle *file, Mplayer_Track_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Artist_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Album_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}
internal void
mplayer_serialize(File_Handle *file, Mplayer_Playlist_ID id)
{
	platform->write_block(file, &id, sizeof(id));
}

internal void
mplayer_serialize(File_Handle *file, u32 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, b32 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, u64 num)
{
	platform->write_block(file, &num, sizeof(num));
}

internal void
mplayer_serialize(File_Handle *file, String8 string)
{
	mplayer_serialize(file, string.len);
	platform->write_block(file, string.str, string.len);
}

internal void
mplayer_serialize(File_Handle *file, Buffer buffer)
{
	mplayer_serialize(file, buffer.size);
	platform->write_block(file, buffer.data, buffer.size);
}

internal void
mplayer_serialize(File_Handle *file, Mplayer_Item_Image image)
{
	mplayer_serialize(file, image.in_disk);
	if (image.in_disk)
	{
		mplayer_serialize(file, image.path);
	}
	else
	{
		mplayer_serialize(file, image.texture_data);
	}
}

internal void
mplayer_serialize(File_Handle *file, Mplayer_Track_ID_List tracks)
{
	mplayer_serialize(file, tracks.count);
	for(Mplayer_Track_ID_Entry *entry = tracks.first;
			entry;
			entry = entry->next)
	{
		mplayer_serialize(file, entry->track_id);
	}
}

internal void
mplayer_serialize_track(File_Handle *file, Mplayer_Track *track)
{
	mplayer_serialize(file, track->hash);
	mplayer_serialize(file, track->path);
	mplayer_serialize(file, track->title);
	mplayer_serialize(file, track->album);
	mplayer_serialize(file, track->artist);
	mplayer_serialize(file, track->genre);
	mplayer_serialize(file, track->date);
	mplayer_serialize(file, track->track_number);
	mplayer_serialize(file, track->start_sample_offset);
	mplayer_serialize(file, track->end_sample_offset);
	mplayer_serialize(file, track->play_count);
}

internal void
mplayer_serialize_playlist(File_Handle *file, Mplayer_Playlist *playlist)
{
	mplayer_serialize(file, playlist->name);
	mplayer_serialize(file, playlist->tracks);
}

internal void
mplayer_serialize_artist(File_Handle *file, Mplayer_Artist *artist)
{
	mplayer_serialize(file, artist->hash);
	mplayer_serialize(file, artist->name);
}

internal Mplayer_Item_Image *mplayer_get_image_by_id(Mplayer_Image_ID image_id, b32 load);

internal void
mplayer_serialize_album(File_Handle *file, Mplayer_Album *album)
{
	mplayer_serialize(file, album->hash);
	mplayer_serialize(file, album->name);
	mplayer_serialize(file, album->artist);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 0);
	mplayer_serialize(file, *image);
}

internal void
mplayer_deserialize_track(Byte_Stream *bs, Mplayer_Track *track)
{
	byte_stream_read(bs, &track->hash);
	byte_stream_read(bs, &track->path);
	byte_stream_read(bs, &track->title);
	byte_stream_read(bs, &track->album);
	byte_stream_read(bs, &track->artist);
	byte_stream_read(bs, &track->genre);
	byte_stream_read(bs, &track->date);
	byte_stream_read(bs, &track->track_number);
	byte_stream_read(bs, &track->start_sample_offset);
	byte_stream_read(bs, &track->end_sample_offset);
	byte_stream_read(bs, &track->play_count);
}

internal void
mplayer_deserialize_artist(Byte_Stream *bs, Mplayer_Artist *artist)
{
	byte_stream_read(bs, &artist->hash);
	byte_stream_read(bs, &artist->name);
}

internal void
mplayer_deserialize_album(Byte_Stream *bs, Mplayer_Album *album)
{
	byte_stream_read(bs, &album->hash);
	byte_stream_read(bs, &album->name);
	byte_stream_read(bs, &album->artist);
	
	Mplayer_Item_Image *image = mplayer_get_image_by_id(album->image_id, 0);
	byte_stream_read(bs, image);
	if (image->in_disk)
	{
		image->path = str8_clone(&mplayer_ctx->library.arena, image->path);
	}
	else
	{
		image->texture_data = clone_buffer(&mplayer_ctx->library.arena, image->texture_data);
	}
}

