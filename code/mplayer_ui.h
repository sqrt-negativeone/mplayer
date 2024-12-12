/* date = November 29th 2024 3:17 pm */

#ifndef MPLAYER_UI_H
#define MPLAYER_UI_H

enum Axis2
{
	Axis2_X,
	Axis2_Y,
	Axis2_COUNT,
};

internal Axis2
inverse_axis(Axis2 axis)
{
	Axis2 inverse = (axis == Axis2_X)? Axis2_Y:Axis2_X;
	return inverse;
}

enum UI_Size_Kind
{
	UI_Size_Kind_Percent,
	UI_Size_Kind_Pixel,
	UI_Size_Kind_Text_Dim,
};

struct UI_Size
{
	UI_Size_Kind kind;
	f32 value;
	f32 strictness;
};

#define UI_DEFAULT_SIZE UI_Size{.kind = UI_SIZE_Percent, .value = 1.0, .strictness = 0.0.}

enum
{
	UI_FLAG_Has_Custom_Draw = (1 << 0),
	UI_FLAG_Draw_Background = (1 << 1),
	UI_FLAG_Draw_Border     = (1 << 2),
	UI_FLAG_Draw_Image      = (1 << 3),
	UI_FLAG_Draw_Text       = (1 << 4),
	UI_FLAG_View_Scroll     = (1 << 5),
	
	UI_FLAG_Left_Mouse_Clickable = (1 << 6),
	
	UI_FLAG_Animate_Pos     = (1 << 7),
	UI_FLAG_Animate_Dim     = (1 << 8),
	UI_FLAG_Animate_Scroll  = (1 << 9),
	
	UI_FLAG_OverflowX       = (1 << 10),
	UI_FLAG_OverflowY       = (1 << 11),
	
	UI_FLAG_Mouse_Clickable = UI_FLAG_Left_Mouse_Clickable,
	UI_FLAG_Clickable       =  UI_FLAG_Mouse_Clickable,
};
typedef u32 UI_Element_Flags;

struct UI_Element;
#define UI_CUSTOM_DRAW_PROC(name) void name(Render_Group *group, UI_Element *element)

typedef UI_CUSTOM_DRAW_PROC(UI_Custom_Draw_Proc);

struct UI_Element
{
	UI_Element *parent;
	UI_Element *next;
	UI_Element *prev;
	UI_Element *first;
	UI_Element *last;
	
	UI_Element *next_free;
	
	UI_Element *next_hash;
	UI_Element *prev_hash;
	u64 id;
	
	u64 frame_index;
	
	Axis2 child_layout_axis;
	UI_Size size[Axis2_COUNT];
	
	UI_Element_Flags flags;
	
	Mplayer_Font *font;
	String8 text;
	Texture texture;
	V4_F32 texture_tint_color;
	V4_F32 background_color;
	V4_F32 text_color;
	
	V4_F32 border_color;
	f32 border_thickness;
	
	V2_F32 rel_top_left_pos; // relative to the parent element
	
	V2_F32 computed_dim;
	Range2_F32 computed_rect;
	
	f32 roundness;
	V2_F32 d_dim;
	V2_F32 dim;
	
	V2_F32 child_bounds;
	Range2_F32 rect;
	
	UI_Custom_Draw_Proc *custom_draw_proc;
	void *custom_draw_data;
	
	V2_F32 scroll_step;
	
	V2_F32 view_target_scroll;
	V2_F32 view_scroll;
	V2_F32 d_view_scroll;
	
	f32 active_t;
	f32 hot_t;
	f32 active_dt;
	f32 hot_dt;
};

struct UI_Elements_Bucket
{
	UI_Element *first;
	UI_Element *last;
};

enum UI_Interaction_Flag
{
	UI_Interaction_Hover    = 1 << 0,
	UI_Interaction_Released = 1 << 1,
	UI_Interaction_Clicked  = 1 << 2,
	UI_Interaction_Pressed  = 1 << 3,
	UI_Interaction_Dragging = 1 << 4,
};

union Mplayer_UI_Interaction
{
	u32 flags;
	struct
	{
		u32 hover : 1;
		u32 released : 1;
		u32 clicked : 1;
		u32 pressed : 1;
		u32 dragging : 1;
	};
};

#define UI_STACK_MAX_SIZE 128
struct UI_Fonts_Stack
{
	Mplayer_Font *stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_Textures_Stack
{
	Texture stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_Color_Stack
{
	V4_F32 stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_V2_F32_Stack
{
	V2_F32 stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_F32_Stack
{
	f32 stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_U32_Stack
{
	u32 stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

struct UI_Size_Stack
{
	UI_Size stack[UI_STACK_MAX_SIZE];
	u32 top;
	b32 auto_pop;
};

#define UI_ELEMENETS_HASHTABLE_SIZE 128
struct Mplayer_UI
{
	Render_Group *group;
	Mplayer_Font *def_font;
	Mplayer_Input *input;
	Memory_Arena *arena;
	Memory_Arena *frame_arena;
	
	u64 frame_index;
	
	V2_F32 mouse_drag_start_pos;
	V2_F32 mouse_pos;
	b32 disable_input;
	
	u64 active_id;
	u64 hot_id;
	
	u64 selected_id;
	
	f32 recent_click_time;
	u32 input_cursor;
	
	u32 grid_first_visible_row;
	u32 grid_item_count_per_row;
	u32 grid_max_rows_count;
	u32 grid_visible_rows_count;
	u32 grid_item_index_in_row;
	i32 grid_row_index;
	f32 grid_vpadding;
	V2_F32 grid_item_dim;
	
	UI_Fonts_Stack    fonts;
	UI_Textures_Stack textures;
	UI_Color_Stack    text_colors;
	UI_Color_Stack    texture_tint_colors;
	UI_Color_Stack    bg_colors;
	UI_Color_Stack    border_colors;
	UI_F32_Stack      border_thickness;
	UI_F32_Stack      roundness_stack;
	UI_U32_Stack      flags_stack;
	UI_V2_F32_Stack   scroll_steps;
	UI_Size_Stack sizes[Axis2_COUNT];
	
	UI_Element *root;
	UI_Element *curr_parent;
	
	UI_Element *free_elements;
	UI_Elements_Bucket elements_table[UI_ELEMENETS_HASHTABLE_SIZE];
};

#endif //MPLAYER_UI_H
