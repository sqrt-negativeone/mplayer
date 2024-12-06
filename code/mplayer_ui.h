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
	UI_FLAG_Clickable       = (1 << 0),
	UI_FLAG_Draw_Background = (1 << 1),
	UI_FLAG_Draw_Border     = (1 << 2),
	UI_FLAG_Draw_Image      = (1 << 3),
	UI_FLAG_Draw_Text       = (1 << 4),
	UI_FLAG_Allow_OverflowX = (1 << 5),
	UI_FLAG_Allow_OverflowY = (1 << 6),
};
typedef u32 UI_Element_Flags;

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
	u64 hash;
	
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
	
	V2_F32 max_child_dim;
	V2_F32 computed_dim;
	V2_F32 computed_pos;
	Range2_F32 rect;
};

struct UI_Elements_Bucket
{
	UI_Element *first;
	UI_Element *last;
};

struct Mplayer_UI_Interaction
{
	b32 hover;
	b32 pressed;
	b32 released;
	b32 clicked; // pressed widget and released inside the widget
	b32 edited;
	b32 not_visible;
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
	
	V2_F32 mouse_p;
	b32 disable_input;
	
	u64 active_widget_id;
	u64 hot_widget_id;
	
	u64 selected_widget_id;
	
	f32 active_t;
	f32 active_dt;
	
	f32 hot_t;
	f32 hot_dt;
	
	f32 recent_click_time;
	u32 input_cursor;
	V2_F32 option_start_pos;
	V2_F32 option_pos;
	V2_F32 option_dim;
	V2_F32 option_padding;
	Range2_F32 option_available_space;
	
	UI_Fonts_Stack    fonts;
	UI_Textures_Stack textures;
	UI_Color_Stack    text_colors;
	UI_Color_Stack    texture_tint_colors;
	UI_Color_Stack    bg_colors;
	UI_Color_Stack    border_colors;
	UI_F32_Stack      border_thickness;
	UI_U32_Stack      flags_stack;
	UI_Size_Stack sizes[Axis2_COUNT];
	
	UI_Element *root;
	UI_Element *curr_parent;
	
	UI_Element *free_elements;
	UI_Elements_Bucket elements_table[UI_ELEMENETS_HASHTABLE_SIZE];
};

#endif //MPLAYER_UI_H
