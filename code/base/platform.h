/* date = December 26th 2024 2:00 pm */

#ifndef PLATFORM_H
#define PLATFORM_H

typedef enum
{
	Cursor_Arrow,
	Cursor_Hand,
	Cursor_Horizontal_Resize,
	Cursor_Vertical_Resize,
	Cursor_Wait,
	Cursor_TextSelect,
	Cursor_Unavailable,
	
	Cursor_COUNT,
} Cursor_Shape;

typedef void Work_Proc(void *data);
struct Work_Entry
{
	Work_Proc *work;
	void *input;
};

#define WORK_SIG(name) void name(void *input)

typedef b32 Push_Work_Proc(Work_Proc *work, void *input);
typedef b32 Do_Next_Work_Proc();

typedef struct File_Iterator_Handle File_Iterator_Handle;
struct File_Iterator_Handle
{
	u8 opaque[kilobytes(1)];
};

enum File_Flag
{
	FileFlag_Directory = (1 << 0),
	FileFlag_Valid     = (1 << 1),
};
typedef u32 File_Flags;

typedef struct File_Info File_Info;
struct File_Info
{
	File_Flags flags;
	String8 path;
	String8 name;
	String8 extension;
	u64 size;
};


typedef u32 File_Open_Flags;
enum
{
	File_Open_Read     = (1 << 0),
	File_Open_Write    = (1 << 1),
	File_Create_Always = (1 << 2),
};

typedef void File_Handle;

typedef struct Directory Directory;
struct Directory
{
	File_Info *files;
	u32 count;
};

union Socket_Handle
{
	u8  opaque[16];
	u64 opaque64[2];
};

internal b32
is_socket_handle_valid(Socket_Handle handle)
{
	b32 valid = handle.opaque64[0] || handle.opaque64[1];
	return valid;
}

typedef b32 Is_Valid_Socket_Proc(Socket_Handle s);
typedef Socket_Handle Accept_Socket_Proc(Socket_Handle s, void *addr, int *addrlen);
typedef void Close_Socket_Proc(Socket_Handle s);
typedef Socket_Handle Connect_To_Server_Proc(const char *server_address, const char *port);
typedef Socket_Handle Open_Listen_Socket_Proc(const char *port);
typedef b32 Network_Send_Buffer_Proc(Socket_Handle s, Buffer buffer);
typedef i32 Network_Receive_Buffer_Proc(Socket_Handle s, Buffer buffer);
typedef i32 Network_Read(Socket_Handle s, Buffer buffer);

typedef b32 Is_Path_Slash_Proc(u8 c);
typedef void Fix_Path_Slashes_Proc(String8 path);
typedef void Make_Folder_If_Missing_Proc(String8 dir);
typedef void Set_Cursor_Proc(Cursor_Shape cursor);

typedef i64 Atomic_Increment64_Proc(volatile i64 *addend);
typedef i64 Atomic_Decrement64_Proc(volatile i64 *addend);
typedef b32 Atomic_Compare_And_Exchange64_Proc(i64 volatile *dst, i64 expect, i64 exchange);
typedef b32 Atomic_Compare_And_Exchange_Pointer_Proc(Address volatile *dst, Address expect, Address exchange);

typedef void     *Allocate_Memory_Proc(u64 size);
typedef void      Deallocate_Memory_Proc(void *memory);
typedef Directory Read_Directory_Proc(Memory_Arena *arena, String8 path);
typedef String8   Get_Current_Directory_Proc(Memory_Arena *arena);
typedef File_Handle *Open_File_Proc(String8 path, u32 flags);
typedef void   Close_File_Proc(File_Handle *file);
typedef Buffer File_Read_block_Proc(File_Handle *file, void *data, u64 size);
typedef b32 File_Write_block_Proc(File_Handle *file, void *buf, u64 size);

typedef Buffer Load_Entire_File(Memory_Arena *arena, String8 file_path);

typedef b32 Open_URL_In_Default_Browser_Proc(String8 url);

typedef struct OS_Vtable OS_Vtable;
struct OS_Vtable
{
	Set_Cursor_Proc                         *set_cursor;
	
	// NOTE(fakhri): file system procs
	Fix_Path_Slashes_Proc                   *fix_path_slashes;
	Is_Path_Slash_Proc                      *is_path_slash;
	Make_Folder_If_Missing_Proc             *make_folder_if_missing;
	Read_Directory_Proc                     *read_directory;
	Get_Current_Directory_Proc              *get_current_directory;
	Load_Entire_File                        *load_entire_file;
	Open_File_Proc                          *open_file;
	Close_File_Proc                         *close_file;
	File_Read_block_Proc                    *read_block;
	File_Write_block_Proc                   *write_block;
	
	// NOTE(fakhri): worker threads procs
	Push_Work_Proc                           *push_work;
	Do_Next_Work_Proc                        *do_next_work;
	
	// NOTE(fakhri): memory procs
	Allocate_Memory_Proc                     *alloc;
	Deallocate_Memory_Proc                   *dealloc;
	
	// NOTE(fakhri): atomic procs
	Atomic_Increment64_Proc                  *atomic_increment64;
	Atomic_Decrement64_Proc                  *atomic_decrement64;
	Atomic_Compare_And_Exchange64_Proc       *atomic_compare_and_exchange64;
	Atomic_Compare_And_Exchange_Pointer_Proc *atomic_compare_and_exchange_pointer;
	
	// NOTE(fakhri): network procs
	Is_Valid_Socket_Proc        *is_valid_socket;
	Accept_Socket_Proc          *accept_socket;
	Close_Socket_Proc           *close_socket;
	Connect_To_Server_Proc      *connect_to_server;
	Open_Listen_Socket_Proc     *open_listen_socket;
	Network_Send_Buffer_Proc    *network_send_buffer;
	Network_Receive_Buffer_Proc *network_receive_buffer;
	Network_Receive_Buffer_Proc *network_read;
	
	Open_URL_In_Default_Browser_Proc         *open_url_in_default_browser;
};

global OS_Vtable *platform = 0;

#endif //PLATFORM_H
