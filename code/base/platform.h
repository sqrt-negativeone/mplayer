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

typedef struct OS_Vtable OS_Vtable;
struct OS_Vtable
{
	Fix_Path_Slashes_Proc                   *fix_path_slashes;
	Is_Path_Slash_Proc                      *is_path_slash;
	Make_Folder_If_Missing_Proc             *make_folder_if_missing;
	Set_Cursor_Proc                         *set_cursor;
	
	Read_Directory_Proc                      *read_directory;
	Get_Current_Directory_Proc               *get_current_directory;
	Load_Entire_File                         *load_entire_file;
	Open_File_Proc                           *open_file;
	Close_File_Proc                          *close_file;
	File_Read_block_Proc                     *read_block;
	File_Write_block_Proc                    *write_block;
	
	Push_Work_Proc                           *push_work;
	Do_Next_Work_Proc                        *do_next_work;
	
	Allocate_Memory_Proc                     *alloc;
	Deallocate_Memory_Proc                   *dealloc;
	
	Atomic_Increment64_Proc                  *atomic_increment64;
	Atomic_Decrement64_Proc                  *atomic_decrement64;
	Atomic_Compare_And_Exchange64_Proc       *atomic_compare_and_exchange64;
	Atomic_Compare_And_Exchange_Pointer_Proc *atomic_compare_and_exchange_pointer;
};

global OS_Vtable *platform = 0;

#endif //PLATFORM_H
