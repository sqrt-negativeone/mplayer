
global _Mplayer_UI *g_ui;

internal UI_Size
ui_make_size(UI_Size_Kind kind, f32 value, f32 strictness)
{
	UI_Size result;
	result.kind       = kind;
	result.value      = value;
	result.strictness = strictness;
	return result;
}

#define ui_stack_top(stack_name) (stack_name).stack[(stack_name).top-1]
#define ui_stack_pop(stack_name) (stack_name).top -= 1;
#define ui_stack_push(stack_name, new_item) do {                 \
assert((stack_name).top < array_count((stack_name).stack));  \
(stack_name).top += 1;                                       \
(stack_name).stack[(stack_name).top - 1] = (new_item);           \
} while(0)

#define ui_stack_push_auto_pop(stack_name, new_item) do {                 \
assert((stack_name).top < array_count((stack_name).stack));  \
(stack_name).top += 1;                                       \
(stack_name).stack[(stack_name).top - 1] = (new_item);           \
(stack_name).auto_pop = 1;           \
} while(0)

#define ui_auto_pop_stack(stack_name) do {                 \
if ((stack_name).auto_pop) {           \
(stack_name).top -= 1;\
(stack_name).auto_pop = 0;\
} \
} while(0)

#define ui_size_percent(percent, strictness) ui_make_size(UI_Size_Kind_Percent, percent, strictness)
#define ui_size_pixel(pixel, strictness)     ui_make_size(UI_Size_Kind_Pixel, pixel, strictness)
#define ui_size_text_dim(strictness)         ui_make_size(UI_Size_Kind_Text_Dim, 0, strictness)
#define ui_size_by_childs(strictness)                 ui_make_size(UI_Size_Kind_By_Childs, 0, strictness)
#define ui_size_parent_remaining() ui_size_percent(1, 0)

#define _defer_loop(exp1, exp2) for(i32 __i__ = ((exp1), 0); __i__ == 0; (__i__ += 1, (exp2)))
#define defer(exp) _defer_loop(0, exp)
#define ui_parent(p) _defer_loop(ui_push_parent(p), ui_pop_parent())
#define ui_vertical_layout()   ui_parent(ui_layout(Axis2_Y))
#define ui_horizontal_layout() ui_parent(ui_layout(Axis2_X))
#define ui_modal(string) _defer_loop(ui_modal_begin(string), ui_modal_end())

#define ui_pref_size(axis, size)                   _defer_loop(ui_push_size(axis, size), ui_pop_size())
#define ui_pref_width(size)                        _defer_loop(ui_push_width(size), ui_pop_width())
#define ui_pref_height(size)                       _defer_loop(ui_push_height(size), ui_pop_height())
#define ui_pref_background_color(bg_color)         _defer_loop(ui_push_background_color(bg_color), ui_pop_background_color())
#define ui_pref_text_color(text_color)             _defer_loop(ui_push_text_color(text_color), ui_pop_text_color())
#define ui_pref_border_color(border_color)         _defer_loop(ui_push_border_color(border_color), ui_pop_border_color())
#define ui_pref_border_thickness(border_thickness) _defer_loop(ui_push_border_thickness(border_thickness), ui_pop_border_thickness())
#define ui_pref_texture(border_thickness)          _defer_loop(ui_push_texture(texture), ui_pop_texture())
#define ui_pref_font(font)                         _defer_loop(ui_push_font(font), ui_pop_font())
#define ui_pref_font_sizes(font_size)              _defer_loop(ui_push_font_sizes(font_size), ui_pop_font_sizes())
#define ui_pref_texture_tint_color(tint_color)     _defer_loop(ui_push_texture_tint_color(tint_color), ui_pop_texture_tint_color())
#define ui_pref_flags(flags)     _defer_loop(ui_push_flags(flags), ui_pop_flags())
#define ui_pref_roundness(roundness)   _defer_loop(ui_push_roundness(roundness), ui_pop_roundness())
#define ui_pref_scroll_step(scroll_step)   _defer_loop(ui_push_scroll_step(scroll_step), ui_pop_scroll_step())
#define ui_pref_hover_cursor(hover_cursor)   _defer_loop(ui_push_hover_cursor(hover_cursor), ui_pop_hover_cursor())
#define ui_pref_childs_axis(axis)   _defer_loop(ui_push_childs_axis(axis), ui_pop_childs_axis())
#define ui_pref_layer(layer)   _defer_loop(ui_push_layer(layer), ui_pop_layer())

#define ui_auto_pop_style() do {              \
ui_auto_pop_stack(g_ui->fonts);               \
ui_auto_pop_stack(g_ui->font_sizes);          \
ui_auto_pop_stack(g_ui->textures);            \
ui_auto_pop_stack(g_ui->text_colors);         \
ui_auto_pop_stack(g_ui->texture_tint_colors); \
ui_auto_pop_stack(g_ui->bg_colors);           \
ui_auto_pop_stack(g_ui->border_colors);       \
ui_auto_pop_stack(g_ui->border_thickness);    \
ui_auto_pop_stack(g_ui->sizes[Axis2_X]);      \
ui_auto_pop_stack(g_ui->sizes[Axis2_Y]);      \
ui_auto_pop_stack(g_ui->flags_stack);         \
ui_auto_pop_stack(g_ui->roundness_stack);     \
ui_auto_pop_stack(g_ui->scroll_steps);        \
ui_auto_pop_stack(g_ui->hover_cursors);       \
ui_auto_pop_stack(g_ui->childs_axis);         \
ui_auto_pop_stack(g_ui->layers);         \
} while (0)

//- NOTE(fakhri): background color stack
internal void
ui_push_background_color(V4_F32 background_color)
{
	ui_stack_push(g_ui->bg_colors, background_color);
}
internal void
ui_next_background_color(V4_F32 background_color)
{
	ui_stack_push_auto_pop(g_ui->bg_colors, background_color);
}

internal void
ui_pop_background_color()
{
	ui_stack_pop(g_ui->bg_colors);
}

// NOTE(fakhri): Size Stack
internal void
ui_push_size(Axis2 axis, UI_Size size)
{
	ui_stack_push(g_ui->sizes[axis], size);
}

internal void
ui_next_size(Axis2 axis, UI_Size size)
{
	ui_stack_push_auto_pop(g_ui->sizes[axis], size);
}

internal void
ui_pop_size(Axis2 axis)
{
	ui_stack_pop(g_ui->sizes[axis]);
}

internal void
ui_push_width(UI_Size size)
{
	ui_push_size(Axis2_X, size);
}

internal void
ui_next_width(UI_Size size)
{
	ui_next_size(Axis2_X, size);
}

internal void
ui_push_height(UI_Size size)
{
	ui_push_size(Axis2_Y, size);
}

internal void
ui_next_height(UI_Size size)
{
	ui_next_size(Axis2_Y, size);
}

internal void
ui_pop_width()
{
	ui_pop_size(Axis2_X);
}

internal void
ui_pop_height()
{
	ui_pop_size(Axis2_Y);
}

//- NOTE(fakhri): text color stack
internal void
ui_push_text_color(V4_F32 text_color)
{
	ui_stack_push(g_ui->text_colors, text_color);
}
internal void
ui_next_text_color(V4_F32 text_color)
{
	ui_stack_push_auto_pop(g_ui->text_colors, text_color);
}

internal void
ui_pop_text_color()
{
	ui_stack_pop(g_ui->text_colors);
}

//- NOTE(fakhri): border color stack
internal void
ui_push_border_color(V4_F32 border_color)
{
	ui_stack_push(g_ui->border_colors, border_color);
}
internal void
ui_next_border_color(V4_F32 border_color)
{
	ui_stack_push_auto_pop(g_ui->border_colors, border_color);
}
internal void
ui_pop_border_color()
{
	ui_stack_pop(g_ui->border_colors);
}

//- NOTE(fakhri): border thickness stack
internal void
ui_push_border_thickness(f32 border_thickness)
{
	ui_stack_push(g_ui->border_thickness, border_thickness);
}
internal void
ui_next_border_thickness(f32 border_thickness)
{
	ui_stack_push_auto_pop(g_ui->border_thickness, border_thickness);
}
internal void
ui_pop_border_thickness()
{
	ui_stack_pop(g_ui->border_thickness);
}

//- NOTE(fakhri): texture stack
internal void
ui_push_texture(Texture texture)
{
	ui_stack_push(g_ui->textures, texture);
}
internal void
ui_next_texture(Texture texture)
{
	ui_stack_push_auto_pop(g_ui->textures, texture);
}
internal void
ui_pop_texture()
{
	ui_stack_pop(g_ui->textures);
}

//- NOTE(fakhri): font stack
internal void
ui_push_font(Font *font)
{
	ui_stack_push(g_ui->fonts, font);
}
internal void
ui_next_font(Font *font)
{
	ui_stack_push_auto_pop(g_ui->fonts, font);
}
internal void
ui_pop_font()
{
	ui_stack_pop(g_ui->fonts);
}

//- NOTE(fakhri): font stack
internal void
ui_push_font_sizes(f32 font_size)
{
	ui_stack_push(g_ui->font_sizes, font_size);
}
internal void
ui_next_font_sizes(f32 font_size)
{
	ui_stack_push_auto_pop(g_ui->font_sizes, font_size);
}
internal void
ui_pop_font_sizes()
{
	ui_stack_pop(g_ui->font_sizes);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_texture_tint_color(V4_F32 tint_color)
{
	ui_stack_push(g_ui->texture_tint_colors, tint_color);
}
internal void
ui_next_texture_tint_color(V4_F32 tint_color)
{
	ui_stack_push_auto_pop(g_ui->texture_tint_colors, tint_color);
}
internal void
ui_pop_texture_tint_color()
{
	ui_stack_pop(g_ui->texture_tint_colors);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_flags(u32 flags)
{
	ui_stack_push(g_ui->flags_stack, flags);
}
internal void
ui_next_flags(u32 flags)
{
	ui_stack_push_auto_pop(g_ui->flags_stack, flags);
}
internal void
ui_pop_flags()
{
	ui_stack_pop(g_ui->flags_stack);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_roundness(f32 roundness)
{
	ui_stack_push(g_ui->roundness_stack, roundness);
}
internal void
ui_next_roundness(f32 roundness)
{
	ui_stack_push_auto_pop(g_ui->roundness_stack, roundness);
}
internal void
ui_pop_roundness()
{
	ui_stack_pop(g_ui->roundness_stack);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_scroll_step(V2_F32 scroll_step)
{
	ui_stack_push(g_ui->scroll_steps, scroll_step);
}
internal void
ui_next_scroll_step(V2_F32 scroll_step)
{
	ui_stack_push_auto_pop(g_ui->scroll_steps, scroll_step);
}
internal void
ui_pop_scroll_step()
{
	ui_stack_pop(g_ui->scroll_steps);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_hover_cursor(u32 hover_cursor)
{
	ui_stack_push(g_ui->hover_cursors, hover_cursor);
}
internal void
ui_next_hover_cursor(u32 hover_cursor)
{
	ui_stack_push_auto_pop(g_ui->hover_cursors, hover_cursor);
}
internal void
ui_pop_hover_cursor()
{
	ui_stack_pop(g_ui->hover_cursors);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_childs_axis(u32 axis)
{
	ui_stack_push(g_ui->childs_axis, axis);
}
internal void
ui_next_childs_axis(u32 axis)
{
	ui_stack_push_auto_pop(g_ui->childs_axis, axis);
}
internal void
ui_pop_childs_axis()
{
	ui_stack_pop(g_ui->childs_axis);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_layer(u32 layer)
{
	ui_stack_push(g_ui->layers, layer);
}
internal void
ui_next_layer(u32 layer)
{
	ui_stack_push_auto_pop(g_ui->layers, layer);
}
internal void
ui_pop_layer()
{
	ui_stack_pop(g_ui->layers);
}

internal void
ui_push_default_style()
{
	ui_push_font(g_ui->def_font);
	ui_push_font_sizes(20.0f);
	ui_push_texture(NULL_TEXTURE);
	ui_push_text_color(vec4(1));
	ui_push_texture_tint_color(vec4(1));
	ui_push_background_color(vec4(0));
	ui_push_border_color(vec4(0,0,0,1));
	ui_push_border_thickness(0);
	ui_push_roundness(0);
	ui_push_flags(0);
	ui_push_scroll_step(vec2(0, -50));
	ui_push_hover_cursor(Cursor_Arrow);
	ui_push_childs_axis(Axis2_Y);
	ui_push_layer(UI_Layer_Default);
	ui_push_width(ui_size_parent_remaining());
	ui_push_height(ui_size_parent_remaining());
}

internal void
ui_reset_default_style()
{
	g_ui->fonts.top = 0;
	g_ui->font_sizes.top = 0;
	g_ui->textures.top = 0;
	g_ui->text_colors.top = 0;
	g_ui->texture_tint_colors.top = 0;
	g_ui->bg_colors.top = 0;
	g_ui->border_colors.top = 0;
	g_ui->border_thickness.top = 0;
	g_ui->roundness_stack.top = 0;
	g_ui->flags_stack.top = 0;
	g_ui->scroll_steps.top = 0;
	g_ui->hover_cursors.top = 0;
	g_ui->childs_axis.top = 0;
	g_ui->layers.top = 0;
	g_ui->sizes[Axis2_X].top = 0;
	g_ui->sizes[Axis2_Y].top = 0;
}


internal void
ui_update_element_interaction(UI_Element *element)
{
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	interaction.element = element;
	Range2_F32 interaction_rect = element->computed_rect;
	u64 id = element->id;
	for (UI_Element *p = element->parent; p; p = p->parent)
	{
		if (p->id)
		{
			interaction_rect = range_intersection(interaction_rect, p->computed_rect);
		}
	}
	
	
	b32 ignore = 0;
	
	if (g_ui->modal_root)
	{
		b32 in_modal = 0;
		for (UI_Element *p = element->parent; p; p = p->parent)
		{
			if (p == g_ui->modal_root)
			{
				in_modal = 1;
				break;
			}
		}
		
		if (!in_modal)
		{
			ignore = 1;
		}
	}
	
	V2_F32 mouse_p = g_ui->mouse_pos;
	b32 mouse_over = is_in_range(interaction_rect, mouse_p) && !ignore;
	
	// TODO(fakhri): Bug: 
	// - click an element
	// - move the mouse over a child of that element
	// - release the mouse, the parent element stays active and is not reset.
	// 
	// If we click somewhere empty and drag the mouse over the element
	// and release we get a click event. this should not happen.
	
	for (Mplayer_Input_Event *e = g_ui->input->first_event, *next = 0; e; e = next)
	{
		next = e->next;
		b32 consumed = 0;
		if (has_flag(element->flags, UI_FLAG_Left_Mouse_Clickable) && e->kind == Event_Kind_Mouse_Move)
		{
			if (mouse_over)
			{
				consumed = 1;
				g_ui->hot_id = id;
			}
			else if (g_ui->hot_id == id)
			{
				consumed = 0;
				g_ui->hot_id = 0;
			}
		}
		
		if (has_flag(element->flags, UI_FLAG_Left_Mouse_Clickable) && e->kind == Event_Kind_Mouse_Key && e->key == Mouse_Left)
		{
			if (mouse_over && e->is_down)
			{
				consumed = 1;
				g_ui->active_id = id;
				g_ui->hot_id = id;
				g_ui->mouse_drag_start_pos = mouse_p;
				g_ui->recent_click_time = g_ui->input->time;
				set_flag(interaction.flags, UI_Interaction_Pressed);
			}
			if (!e->is_down)
			{
				if (g_ui->hot_id == id)
				{
					consumed = 1;
					if (g_ui->active_id == id)
						set_flag(interaction.flags, UI_Interaction_Clicked);
					g_ui->hot_id = 0;
				}
				
				if (g_ui->active_id == id)
				{
					consumed = 1;
					set_flag(interaction.flags, UI_Interaction_Released);
					g_ui->active_id = 0;
				}
				
				if (has_flag(element->flags, UI_FLAG_Selectable))
				{
					if (!mouse_over && g_ui->selected_id == id) g_ui->selected_id = 0;
				}
				
			}
		}
		
		if (mouse_over && has_flag(element->flags, UI_FLAG_View_Scroll) && e->kind == Event_Kind_Mouse_Wheel)
		{
			consumed = 1;
			element->view_target_scroll += element->scroll_step * e->scroll;
			
			V2_F32 bounds_dim = element->child_bounds;
			element->view_target_scroll.x = CLAMP(0, element->view_target_scroll.x, bounds_dim.width);
			element->view_target_scroll.y = CLAMP(0, element->view_target_scroll.y, bounds_dim.height);
		}
		
		if (consumed)
		{
			DLLRemove(g_ui->input->first_event, g_ui->input->last_event, e);
		}
	}
	
	if (mouse_over)
	{
		set_flag(interaction.flags, UI_Interaction_Hover);
	}
	else
	{
		if (g_ui->hot_id == id)
		{
			g_ui->hot_id = 0;
		}
	}
	
	if (g_ui->active_id == id)
	{
		set_flag(interaction.flags, UI_Interaction_Pressed);
	}
	
	if (has_flag(element->flags, UI_FLAG_Selectable))
	{
		if (interaction.clicked)
		{
			g_ui->selected_id = id;
		}
		if (!mouse_over && interaction.released && g_ui->selected_id == id)
		{
			g_ui->selected_id = 0;
		}
		
		if (g_ui->selected_id == id)
		{
			set_flag(interaction.flags, UI_Interaction_Selected);
		}
	}
	
	element->last_frame_interaction = interaction;
}

internal Mplayer_UI_Interaction
ui_interaction_from_element(UI_Element *element)
{
	Mplayer_UI_Interaction interaction = element->last_frame_interaction;
	return interaction;
}

internal void
ui_push_parent(UI_Element *node)
{
	g_ui->curr_parent = node;
}

internal void
ui_pop_parent()
{
	UI_Element *parent = g_ui->curr_parent;
	if (parent)
	{
		g_ui->curr_parent = parent->parent;
	}
}

// NOTE(fakhri): from http://www.cse.yorku.ca/~oz/hash.html
// TODO(fakhri): I just noticed this is the same as str8_hash function from mplayer.cpp but seeded with 5381 (choosen randomly lol)
// change str8_hash to make it accept a seed and use that instead of having two string hashing functions that do the same thing
internal u64
ui_hash_key(String8 key)
{
	u64 hash = 0;
	if (key.len)
	{
		hash = 5381;
		for (u32 i = 0; i < key.len; i += 1)
		{
			hash = ((hash << 5) + hash) + key.str[i];
		}
	}
	return hash;
}


internal UI_Element *
ui_element_by_id(u64 id)
{
	UI_Element *result = 0;
	u32 bucket_index = id % array_count(g_ui->elements_table);
	if (id)
	{
		for (UI_Element *ht_node = g_ui->elements_table[bucket_index].first; ht_node; ht_node = ht_node->next_hash)
		{
			if (ht_node->id == id)
			{
				result = ht_node;
				break;
			}
		}
	}
	
	return result;
}

internal String8
ui_drawable_text_from_string(String8 string)
{
	String8 result = string;
	u64 sep_pos = find_substr8(string, str8_lit("##"), 0, 0);
	if (sep_pos < string.len)
	{
		result.len = sep_pos;
	}
	return result;
}

internal String8
ui_key_from_string(String8 string)
{
	String8 result = string;
	u64 sep_pos = find_substr8(string, str8_lit("###"), 0, 0);
	if (sep_pos < string.len)
	{
		result = str8_skip_first(string, sep_pos);
	}
	return result;
}

internal void
ui_element_set_draw_proc(UI_Element *element, UI_Custom_Draw_Proc *draw_proc, void *custom_draw_data)
{
	set_flag(element->flags, UI_FLAG_Has_Custom_Draw);
	element->custom_draw_proc = draw_proc;
	element->custom_draw_data = custom_draw_data;
}

internal void
ui_element_set_text(UI_Element *element, String8 string)
{
	element->text = ui_drawable_text_from_string(string);
}

internal UI_Element *
ui_element(String8 string, UI_Element_Flags flags = 0)
{
	UI_Element *result = 0;
	
	//- NOTE(fakhri): check hash table
	String8 key = ui_key_from_string(string);
	u64 id = ui_hash_key(key);
	u32 bucket_index = id % array_count(g_ui->elements_table);
	if (id)
	{
		for (UI_Element *ht_node = g_ui->elements_table[bucket_index].first; ht_node; ht_node = ht_node->next_hash)
		{
			if (ht_node->id == id && ht_node->frame_index < g_ui->frame_index)
			{
				result = ht_node;
				break;
			}
		}
	}
	
	if (!result)
	{
		//- NOTE(fakhri): create new element
		result = g_ui->free_elements;
		if (result)
		{
			g_ui->free_elements = result->next_free;
			memory_zero_struct(result);
		}
		else
		{
			result = m_arena_push_struct_aligned(g_ui->arena, UI_Element, 16);
			memory_zero_struct(result);
		}
		
		// NOTE(fakhri): push to the hash table
		DLLPushBack_NP(g_ui->elements_table[bucket_index].first, g_ui->elements_table[bucket_index].last, result, next_hash, prev_hash);
	}
	
	//- NOTE(fakhri): init ui element
	assert(result);
	result->flags = flags;
	result->size[0]  = ui_stack_top(g_ui->sizes[0]);
	result->size[1]  = ui_stack_top(g_ui->sizes[1]);
	result->id = id;
	result->frame_index = g_ui->frame_index;
	result->parent = result->next = result->prev = result->first = result->last = 0;
	
	result->layer = (UI_Layer)ui_stack_top(g_ui->layers);
	result->hover_cursor = (Cursor_Shape)ui_stack_top(g_ui->hover_cursors);
	result->child_layout_axis = (Axis2)ui_stack_top(g_ui->childs_axis);
	result->flags |= ui_stack_top(g_ui->flags_stack);
	result->roundness = ui_stack_top(g_ui->roundness_stack);
	
	if (result->last_frame_interaction.element != result)
	{
		result->last_frame_interaction = ZERO_STRUCT;
		result->last_frame_interaction.element = result;
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Background))
	{
		result->background_color = ui_stack_top(g_ui->bg_colors);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Border))
	{
		result->border_color     = ui_stack_top(g_ui->border_colors);
		result->border_thickness = ui_stack_top(g_ui->border_thickness);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Image))
	{
		result->texture = ui_stack_top(g_ui->textures);
		result->texture_tint_color = ui_stack_top(g_ui->texture_tint_colors);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Text))
	{
		result->font       = ui_stack_top(g_ui->fonts);
		result->font_size  = ui_stack_top(g_ui->font_sizes);
		result->text_color = ui_stack_top(g_ui->text_colors);
		result->text       = ui_drawable_text_from_string(string);
	}
	
	if (has_flag(flags, UI_FLAG_View_Scroll))
	{
		result->scroll_step = ui_stack_top(g_ui->scroll_steps);
	}
	
	if (g_ui->curr_parent)
	{
		DLLPushBack(g_ui->curr_parent->first, g_ui->curr_parent->last, result);
		result->parent = g_ui->curr_parent;
	}
	
	// ui_element_push(result);
	
	ui_auto_pop_style();
	return result;
}

internal UI_Element *
ui_element_f(UI_Element_Flags flags, const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(g_ui->frame_arena, fmt, args);
	va_end(args);
	
	UI_Element *result = ui_element(string, flags);
	return result;
}


//- NOTE(fakhri): layout
internal UI_Element *
ui_layout(Axis2 child_layout_axis)
{
	ui_next_childs_axis(child_layout_axis);
	UI_Element *layout = ui_element(str8_lit(""), UI_FLAG_Draw_Background | UI_FLAG_Draw_Border);
	return layout;
}

internal void
ui_spacer(UI_Size size)
{
	UI_Element *parent = g_ui->curr_parent;
	if (parent)
	{
		ui_next_size(parent->child_layout_axis, size);
		ui_next_size(inverse_axis(parent->child_layout_axis), ui_size_pixel(0, 0));
		UI_Element *spacer = ui_element(str8_lit(""), 0);
	}
}

#define ui_spacer_pixels(px, strictness) ui_spacer(ui_size_pixel(px, strictness))
#define ui_padding(size) _defer_loop(ui_spacer(size), ui_spacer(size))

internal UI_Element *
ui_label(String8 string)
{
	UI_Element *label = ui_element(ZERO_STRUCT,
		UI_FLAG_Draw_Text |
		UI_FLAG_Draw_Border |
		UI_FLAG_Draw_Background
	);
	
	label->text = str8_clone(g_ui->frame_arena, ui_drawable_text_from_string(string));
	return label;
}

internal Mplayer_UI_Interaction
ui_button(String8 string)
{
	ui_next_hover_cursor(Cursor_Hand);
	UI_Element *button = ui_element(string,
		UI_FLAG_Clickable |
		UI_FLAG_Draw_Background |
		UI_FLAG_Draw_Text
	);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(button);
	
	return interaction;
}

internal Mplayer_UI_Interaction
ui_button_f(const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(g_ui->frame_arena, fmt, args);
	va_end(args);
	
	Mplayer_UI_Interaction interaction = ui_button(string);
	return interaction;
}


struct UI_Input_Field_Draw_Data
{
	f32 cursor_offset_x;
	V2_F32 cursor_dim;
};

UI_CUSTOM_DRAW_PROC(ui_input_field_default_draw)
{
	UI_Input_Field_Draw_Data *data = (UI_Input_Field_Draw_Data *)element->custom_draw_data;
	
	// NOTE(fakhri): draw cursor
	if (g_ui->selected_id == element->id)
	{
		V2_F32 pos = range_center(element->rect);
		f32 blink_t = (sin_f(2 * PI32 * (g_ui->input->time - g_ui->recent_click_time)) + 1) / 2;
		if (g_ui->input->time - g_ui->recent_click_time > 5)
		{
			blink_t = 1.0f;
		}
		
		V4_F32 cursor_color = lerp(element->background_color,
			blink_t,
			vec4(0, 0, 0, 1));
		
		V3_F32 cursor_pos = vec3(pos.x + data->cursor_offset_x,
			pos.y,
			(f32)element->layer);
		
		push_rect(group, cursor_pos, data->cursor_dim, cursor_color, 0);
	}
}

internal b32
ui_input_field(String8 key, String8 *buffer, u64 max_capacity)
{
	b32 edited = 0;
	ui_next_childs_axis(Axis2_Y);
	ui_next_hover_cursor(Cursor_TextSelect);
	UI_Element *text_input = ui_element(key, 
		UI_FLAG_Draw_Background|UI_FLAG_Draw_Border|UI_FLAG_Clickable|UI_FLAG_Selectable);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(text_input);
	if (interaction.hover)
	{
		text_input->background_color = vec4(0.3f, 0.3f, 0.3f, 1);
	}
	
	if (interaction.selected)
	{
		text_input->background_color = vec4(0.35f, 0.35f, 0.35f, 1);
		
		Font *font = ui_stack_top(g_ui->fonts);
		f32 font_size = ui_stack_top(g_ui->font_sizes);
		
		V2_F32 text_dim = fnt_compute_text_dim(font, font_size, *buffer);
		if (interaction.clicked || interaction.pressed)
		{
			V2_F32 pos = range_center(text_input->rect);
			f32 test_x = pos.x - 0.5f * text_dim.width;
			g_ui->input_cursor = 0;
			
			for (String8_UTF8_Iterator it = str8_utf8_iterator(*buffer);
				str8_utf8_it_valid(&it);
				str8_utf8_advance(&it))
			{
				Glyph_Metrics *glyph = fnt_get_glyph(font, font_size, it.utf8.codepoint);
				f32 glyph_width = glyph->advance;
				if (test_x + 0.5f * glyph_width < g_ui->mouse_pos.x)
				{
					g_ui->input_cursor = u32(it.offset + it.utf8.advance);
				}
				else break;
				
				test_x += glyph_width;
			}
		}
		
		for (Mplayer_Input_Event *event = g_ui->input->first_event; event; event = event->next)
		{
			b32 consumed = 0;
			switch(event->kind)
			{
				// TODO(fakhri): text selection
				// TODO(fakhri): handle standard keyboard shortcuts (ctrl+arrows, ctrl+backspace...)
				case Event_Kind_Text:
				{
					edited = 1;
					consumed = true;
					if (buffer->len < max_capacity)
					{
						if (g_ui->input_cursor < buffer->len)
						{
							memory_move(buffer->str + g_ui->input_cursor + 1,
								buffer->str + g_ui->input_cursor,
								buffer->len - g_ui->input_cursor);
						}
						
						buffer->str[g_ui->input_cursor] = (u8)event->text_character;
						buffer->len += 1;
						g_ui->input_cursor += 1;
					}
				};
				case Event_Kind_Keyboard_Key:
				{
					if (event->is_down)
					{
						if (event->key == Keyboard_Backspace)
						{
							edited = 1;
							consumed = true;
							if (g_ui->input_cursor && buffer->len)
							{
								memory_move(buffer->str + g_ui->input_cursor - 1,
									buffer->str + g_ui->input_cursor,
									buffer->len - g_ui->input_cursor);
								buffer->len     -= 1;
								g_ui->input_cursor -= 1;
							}
						}
						else if (event->key == Keyboard_Left)
						{
							consumed = true;
							if (g_ui->input_cursor)
								g_ui->input_cursor -= 1;
						}
						else if (event->key == Keyboard_Right)
						{
							consumed = true;
							if (g_ui->input_cursor < buffer->len)
								g_ui->input_cursor += 1;
						}
					}
					
				} break;
				
				case Event_Kind_Mouse_Key: break;
				case Event_Kind_Mouse_Wheel: break;
				case Event_Kind_Mouse_Move: break;
				case Event_Kind_Null: break;
				invalid_default_case;
			}
			
			if (consumed)
			{
				DLLRemove(g_ui->input->first_event, g_ui->input->last_event, event);
			}
		}
		if (g_ui->input_cursor > buffer->len)
		{
			g_ui->input_cursor = (u32)buffer->len;
		}
		
		
		UI_Input_Field_Draw_Data *draw_data = m_arena_push_struct_z(g_ui->frame_arena, UI_Input_Field_Draw_Data);
		draw_data->cursor_offset_x = -0.5f * text_dim.width + fnt_compute_text_dim(font, font_size, str8(buffer->str, MIN(buffer->len, g_ui->input_cursor))).width;
		draw_data->cursor_dim = vec2(2.1f, 1.25f * text_dim.height);
		ui_element_set_draw_proc(text_input, ui_input_field_default_draw, draw_data);
	}
	
	ui_parent(text_input) ui_padding(ui_size_parent_remaining())
	{
		ui_next_height(ui_size_text_dim(1));
		UI_Element *text = ui_label(*buffer);
	}
	
	return edited;
}


struct UI_Slider_Draw_Data
{
	f32 percent;
};

internal
UI_CUSTOM_DRAW_PROC(ui_slider_default_draw_proc)
{
	UI_Slider_Draw_Data *slider_data = (UI_Slider_Draw_Data *)element->custom_draw_data;
	
	V2_F32 slider_dim = element->dim;
	slider_dim.height = lerp(element->dim.height, element->hot_t, 1.3f * element->dim.height);
	slider_dim.height = lerp(slider_dim.height, element->active_t, 0.9f * element->dim.height);
	
	V2_F32 slider_pos = range_center(element->rect);
	
	push_rect(group, element->rect, element->background_color, element->roundness);
	
	V4_F32 progress_bg_color = vec4(0.6f, 0.6f, 0.6f, 1);
	
	V2_F32 full_progress_dim = slider_dim;
	full_progress_dim.width  *= 0.99f;
	full_progress_dim.height *= 0.3f;
	
	push_rect(group, slider_pos, full_progress_dim, progress_bg_color, 5);
	
	Range2_F32 filled_progress_rect = range_center_dim(slider_pos, full_progress_dim);
	filled_progress_rect.max_x -= full_progress_dim.width * (1 - slider_data->percent);
	V4_F32 filled_progress_color = vec4(1, 1, 1, 1);
	push_rect(group, filled_progress_rect, filled_progress_color, 5);
	
}

internal Mplayer_UI_Interaction
ui_slider_f32(String8 string, f32 *val, f32 min, f32 max)
{
	ui_next_hover_cursor(Cursor_Hand);
	UI_Element *slider = ui_element(string, UI_FLAG_Draw_Background | UI_FLAG_Clickable);
	f32 percent = map_into_range_zo(min, *val, max);
	
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(slider);
	if (interaction.pressed)
	{
		percent = map_into_range_zo(slider->computed_rect.min_x, 
			g_ui->mouse_pos.x,
			slider->computed_rect.max_x);
	}
	
	UI_Slider_Draw_Data *slider_data = m_arena_push_struct_z(g_ui->frame_arena, UI_Slider_Draw_Data);
	slider_data->percent = percent;
	ui_element_set_draw_proc(slider, ui_slider_default_draw_proc, slider_data);
	
	*val = map_into_range(percent, 0, 1, min, max);
	return interaction;
}

internal Mplayer_UI_Interaction
ui_slider_u64(String8 string, u64 *val, u64 min, u64 max)
{
	ui_next_hover_cursor(Cursor_Hand);
	UI_Element *slider = ui_element(string, UI_FLAG_Draw_Background | UI_FLAG_Clickable);
	f32 percent = (f32)map_into_range_zo((f64)min, (f64)*val, (f64)max);
	
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(slider);
	if (interaction.pressed)
	{
		percent = map_into_range_zo(slider->computed_rect.min_x, 
			g_ui->mouse_pos.x,
			slider->computed_rect.max_x);
	}
	
	UI_Slider_Draw_Data *slider_data = m_arena_push_struct_z(g_ui->frame_arena, UI_Slider_Draw_Data);
	slider_data->percent = percent;
	ui_element_set_draw_proc(slider, ui_slider_default_draw_proc, slider_data);
	
	*val = u64(min + percent * (max - min));
	*val = CLAMP(min, *val, max);
	return interaction;
}


internal void
ui_grid_push_row()
{
	ui_next_height(ui_size_pixel(g_ui->grid_item_dim.height, 1));
	ui_push_parent(ui_layout(Axis2_X));
	ui_spacer(ui_size_parent_remaining());
	g_ui->grid_item_index_in_row = 0;
}

internal b32
ui_grid_item_begin()
{
	b32 good = 0;
	if (g_ui->grid_item_index < g_ui->grid_items_count && 
		g_ui->grid_item_index_in_row < g_ui->grid_item_count_per_row)
	{
		good = 1;
		
		g_ui->grid_item_index += 1;
		g_ui->grid_item_index_in_row += 1;
		ui_next_width(ui_size_pixel(g_ui->grid_item_dim.width, 1));
		ui_next_height(ui_size_pixel(g_ui->grid_item_dim.height, 1));
		UI_Element *grid_item = ui_element({0}, 0);
		ui_push_parent(grid_item);
	}
	return good;
}


internal void
ui_grid_item_end()
{
	// NOTE(fakhri): pop grid item
	ui_pop_parent();
	ui_spacer(ui_size_parent_remaining());
	
	// NOTE(fakhri): check if row is full
	if (g_ui->grid_item_index_in_row == g_ui->grid_item_count_per_row)
	{
		ui_pop_parent(); // pop parent row
		ui_spacer_pixels(g_ui->grid_vpadding, 1);
		g_ui->grid_row_index += 1;
		
		if (g_ui->grid_row_index < (i32)g_ui->grid_visible_rows_count)
		{
			ui_grid_push_row();
		}
	}
}

internal u32
ui_grid_begin(String8 string, u32 items_count, V2_F32 grid_item_dim, f32 vpadding)
{
	ui_next_childs_axis(Axis2_Y);
	UI_Element *grid = ui_element(string, UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	ui_push_parent(grid);
	
	u32 item_count_per_row = (u32)((grid->computed_dim.width) / grid_item_dim.width);
	item_count_per_row = MAX(item_count_per_row, 1);
	
	u32 max_rows_count = ((items_count + item_count_per_row - 1) / item_count_per_row);
	
	u32 visible_rows_count = u32(grid->computed_dim.height / (grid_item_dim.height + vpadding)) + 2;
	
	f32 space_before_first_row = ABS(grid->view_scroll.y);
	u32 first_visible_row = (u32)(space_before_first_row / (grid_item_dim.height + vpadding));
	first_visible_row = MIN(first_visible_row, max_rows_count-1);
	
	ui_spacer_pixels(first_visible_row * (grid_item_dim.height + vpadding), 1);
	
	g_ui->grid_items_count = items_count;
	g_ui->grid_first_visible_row = first_visible_row;
	g_ui->grid_item_count_per_row = item_count_per_row;
	g_ui->grid_max_rows_count = max_rows_count;
	g_ui->grid_item_index_in_row = 0;
	g_ui->grid_row_index = 0;
	g_ui->grid_visible_rows_count = visible_rows_count;
	g_ui->grid_vpadding = vpadding;
	g_ui->grid_item_dim = grid_item_dim;
	
	if (items_count)
	{
		ui_grid_push_row();
	}
	
	g_ui->grid_item_index = first_visible_row * item_count_per_row;
	return g_ui->grid_item_index;
}

internal void
ui_grid_end()
{
	// NOTE(fakhri): pop incomplete rows
	if (g_ui->grid_row_index < (i32)g_ui->grid_visible_rows_count && 
		g_ui->grid_item_index_in_row < g_ui->grid_item_count_per_row)
	{
		ui_pop_parent(); // pop parent row
	}
	
	u32 rows_remaining = g_ui->grid_max_rows_count - MIN(g_ui->grid_first_visible_row + g_ui->grid_visible_rows_count, g_ui->grid_max_rows_count);
	ui_spacer_pixels(rows_remaining * (g_ui->grid_item_dim.height + g_ui->grid_vpadding), 1);
	
	// NOTE(fakhri): pop grid
	ui_pop_parent();
}

#define ui_for_each_grid_item(string, items_count, item_dim, vpadding, item_index) \
defer(ui_grid_end()) for (u32 item_index = ui_grid_begin(string, items_count, item_dim, vpadding); \
ui_grid_item_begin();\
(item_index += 1, ui_grid_item_end()))

#define ui_for_each_list_item(string, items_count, item_height, vpadding, item_index) \
defer(ui_list_end()) for (u32 item_index = ui_list_begin(string, items_count, item_height, vpadding); \
ui_list_item_begin();\
(item_index += 1, ui_list_item_end()))


internal b32
ui_list_item_begin()
{
	b32 good = 0;
	if (g_ui->list_item_index < g_ui->list_items_count && 
		g_ui->list_row_index < g_ui->list_visible_rows_count)
	{
		good = 1;
		
		g_ui->list_item_index += 1;
		g_ui->list_row_index += 1;
		
		ui_next_width(ui_size_parent_remaining());
		ui_next_height(ui_size_pixel(g_ui->list_item_height, 1));
		UI_Element *list_item = ui_element({0}, 0);
		
		ui_push_parent(list_item);
	}
	return good;
}

internal void
ui_list_item_end()
{
	ui_pop_parent();
	ui_spacer_pixels(g_ui->list_vpadding, 1);
}

internal u32
ui_list_begin(String8 string, u32 items_count, f32 item_height, f32 vpadding)
{
	UI_Element *list = ui_element(string, UI_FLAG_Draw_Border | UI_FLAG_Draw_Background | UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	list->child_layout_axis = Axis2_Y;
	ui_push_parent(list);
	
	u32 visible_rows_count = u32(list->computed_dim.height / (item_height + vpadding)) + 2;
	
	f32 space_before_first_row = ABS(list->view_scroll.y);
	u32 first_visible_row = (u32)(space_before_first_row / (item_height + vpadding));
	first_visible_row = MIN(first_visible_row, items_count);
	
	ui_spacer_pixels(first_visible_row * (item_height + vpadding), 1);
	
	g_ui->list_items_count = items_count;
	g_ui->list_first_visible_row = first_visible_row;
	g_ui->list_max_rows_count = items_count;
	g_ui->list_row_index = 0;
	g_ui->list_visible_rows_count = visible_rows_count;
	g_ui->list_vpadding = vpadding;
	g_ui->list_item_height = item_height;
	
	g_ui->list_item_index = first_visible_row;
	return g_ui->list_item_index;
}

internal void
ui_list_end()
{
	u32 rows_remaining = g_ui->list_max_rows_count - MIN(g_ui->list_first_visible_row + g_ui->list_visible_rows_count, g_ui->list_max_rows_count);
	ui_spacer_pixels(rows_remaining * (g_ui->list_item_height + g_ui->list_vpadding), 1);
	
	// NOTE(fakhri): pop list
	ui_pop_parent();
}


internal void
ui_layout_compute_preorder_sizes(UI_Element *node, Axis2 axis)
{
	//- NOTE(fakhri): computation fixed sizes and upward dependent sizes
	switch(node->size[axis].kind)
	{
		case UI_Size_Kind_Pixel:
		{
			node->computed_dim.v[axis] = node->size[axis].value;
		} break;
		
		case UI_Size_Kind_Text_Dim:
		{
			node->computed_dim.v[axis] = fnt_compute_text_dim(node->font, node->font_size, node->text).v[axis];
		} break;
		case UI_Size_Kind_Percent:
		{
			UI_Element *p = node->parent;
			
			for(;p; p = p->parent)
			{
				if (p->size[axis].kind != UI_Size_Kind_By_Childs)
				{
					break;
				}
			}
			
			assert(p);
			node->computed_dim.v[axis] = node->size[axis].value * p->computed_dim.v[axis];
		} break;
		case UI_Size_Kind_By_Childs: break;
	}
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_layout_compute_preorder_sizes(child, axis);
	}
}

internal void
ui_layout_compute_postorder_sizes(UI_Element *node, Axis2 axis)
{
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_layout_compute_postorder_sizes(child, axis);
	}
	
	//- NOTE(fakhri): computation fixed sizes and upward dependent sizes
	switch(node->size[axis].kind)
	{
		case UI_Size_Kind_By_Childs: 
		{
			f32 value = 0;
			if (axis == node->child_layout_axis)
			{
				for (UI_Element *child = node->first; child; child = child->next)
				{
					value += child->computed_dim.v[axis];
				}
			}
			else
			{
				for (UI_Element *child = node->first; child; child = child->next)
				{
					value = MAX(value, child->computed_dim.v[axis]);
				}
			}
			
			node->computed_dim.v[axis] = value;
		} break;
		
		case UI_Size_Kind_Pixel:
		case UI_Size_Kind_Text_Dim:
		case UI_Size_Kind_Percent: break;
	}
	
}

internal void
ui_layout_fix_sizes_violations(UI_Element *node, Axis2 axis)
{
	if (!node->first) return;
	
	f32 parent_size = node->computed_dim.v[axis];
	if (!has_flag(node->flags, UI_FLAG_OverflowX << axis))
	{
		if (axis == node->child_layout_axis)
		{
			f32 total_fixup = 0;
			f32 childs_sum_size = 0;
			for (UI_Element *child = node->first; child; child = child->next)
			{
				if (has_flag(child->flags, UI_FLAG_FloatX<<axis)) continue;
				
				total_fixup += child->computed_dim.v[axis] * (1 - child->size[axis].strictness);
				childs_sum_size += child->computed_dim.v[axis];
			}
			
			if (childs_sum_size > parent_size && total_fixup > 0)
			{
				f32 extra_size = childs_sum_size - parent_size;
				
				for (UI_Element *child = node->first; child && childs_sum_size > parent_size; child = child->next)
				{
					if (has_flag(child->flags, UI_FLAG_FloatX<<axis)) continue;
					
					f32 child_fixup = child->computed_dim.v[axis] * (1 - child->size[axis].strictness);
					f32 balanced_child_fixup = child_fixup * extra_size / total_fixup;
					child_fixup = CLAMP(0, balanced_child_fixup, child_fixup);
					child->computed_dim.v[axis] -= child_fixup;
				}
			}
		}
		else
		{
			for (UI_Element *child = node->first; child; child = child->next)
			{
				if (has_flag(child->flags, UI_FLAG_FloatX<<axis)) continue;
				
				if (child->computed_dim.v[axis] > parent_size)
				{
					child->computed_dim.v[axis] = MAX(child->size[axis].strictness * child->computed_dim.v[axis], parent_size);
				}
			}
		}
	}
	
	// NOTE(fakhri): position childs
	{
		if (axis == node->child_layout_axis)
		{
			f32 p = 0;
			node->child_bounds.v[axis] = 0;
			for (UI_Element *child = node->first; child; child = child->next)
			{
				if (has_flag(child->flags, UI_FLAG_FloatX<<axis)) continue;
				child->rel_top_left_pos.v[axis] = p;
				p += (axis==Axis2_Y? -1:1)*child->computed_dim.v[axis];
				node->child_bounds.v[axis] += child->computed_dim.v[axis];
			}
		}
		else
		{
			for (UI_Element *child = node->first; child; child = child->next)
			{
				if (has_flag(child->flags, UI_FLAG_FloatX<<axis)) continue;
				child->rel_top_left_pos.v[axis] = 0;
				node->child_bounds.v[axis] = MAX(child->computed_dim.v[axis], child->computed_dim.v[axis]);
			}
		}
		
		// NOTE(fakhri): compute on screen position from relative positions
		f32 parent_origin = (axis == Axis2_X)? node->computed_rect.min_x:node->computed_rect.max_y;
		parent_origin += ((axis == Axis2_Y)? 1:-1) * node->view_scroll.v[axis];
		for (UI_Element *child = node->first; child; child = child->next)
		{
			f32 start_offset = parent_origin;
			if (axis == Axis2_Y)
				start_offset -= child->computed_dim.height;
			
			child->computed_rect.minp.v[axis] = start_offset + child->rel_top_left_pos.v[axis];
			child->computed_rect.maxp.v[axis]= child->computed_rect.minp.v[axis] + child->computed_dim.v[axis];
		}
	}
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_layout_fix_sizes_violations(child, axis);
	}
}


internal void
ui_update_layout(UI_Element *root)
{
	for (u32 axis = Axis2_X; axis < Axis2_COUNT; axis += 1)
	{
		ui_layout_compute_preorder_sizes(root, (Axis2)axis);
		ui_layout_compute_postorder_sizes(root, (Axis2)axis);
		ui_layout_fix_sizes_violations(root, (Axis2)axis);
	}
}

internal void
ui_draw_elements(Render_Group *group, UI_Element *node)
{
	Range2_F32 old_cull = group->config.cull_range;
	
	if (has_flag(node->flags, UI_FLAG_Clip))
	{
		render_group_add_cull_range(group, node->rect);
	}
	
	V3_F32 pos = vec3(range_center(node->rect), (f32)node->layer);
	// NOTE(fakhri): only render the leaf nodes
	if (has_flag(node->flags, UI_FLAG_Draw_Background))
	{
		push_rect(group, pos, node->dim, node->background_color, node->roundness);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Image))
	{
		push_image(group, pos, node->dim, node->texture, node->texture_tint_color, node->roundness);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Text))
	{
		fnt_draw_text(group, node->font, node->font_size, node->text, pos, node->text_color, 
			Text_Render_Flag_Centered);
	}
	
	if (has_flag(node->flags, UI_FLAG_Has_Custom_Draw))
	{
		node->custom_draw_proc(group, node);
	}
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_draw_elements(group, child);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Border))
	{
		draw_outline(group, pos, node->dim, node->border_color, node->border_thickness);
	}
	
	
	if (has_flag(node->flags, UI_FLAG_Clip))
	{
		render_group_set_cull_range(group, old_cull);
	}
	
}

internal void
ui_cache_or_dispose_hierarchy(UI_Element *node)
{
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_cache_or_dispose_hierarchy(child);
	}
	
	// NOTE(fakhri): dispose of node
	node->next_free = g_ui->free_elements;
	g_ui->free_elements = node;
}

internal void
ui_animate_elements(UI_Element *node)
{
	f32 dt = g_ui->input->frame_dt;
	
	f32 step42 = 1 - pow_f(2, -42 * dt);
	f32 step69 = 1 - pow_f(2, -69 * dt);
	
	if (node->id)
	{
		//- NOTE(fakhri): animate position
		if (has_flag(node->flags, UI_FLAG_Animate_Pos))
		{
			V2_F32 pos = range_center(node->rect);
			V2_F32 target_pos = range_center(node->computed_rect);
			
			pos += (target_pos - pos) * step42;
			node->rect = range_center_dim(pos, node->dim);
		}
		else
		{
			node->rect = range_center_dim(range_center(node->computed_rect), node->dim);
		}
		
		//- NOTE(fakhri): animate dim
		if (has_flag(node->flags, UI_FLAG_Animate_Dim))
		{
			node->dim += (node->computed_dim - node->dim) * step42;
			if (ABS(node->dim.x - node->computed_dim.x) < 1)
			{
				node->dim.x = node->computed_dim.x;
			}
			if (ABS(node->dim.y - node->computed_dim.y) < 1)
			{
				node->dim.y = node->computed_dim.y;
			}
		}
		else
		{
			node->dim = node->computed_dim;
		}
		
		//- NOTE(fakhri): animate scroll
		if (has_flag(node->flags, UI_FLAG_Animate_Scroll))
		{
			node->view_scroll += (node->view_target_scroll - node->view_scroll) * step69;
			
			if (ABS(node->view_scroll.x - node->view_target_scroll.x) < 1)
			{
				node->view_scroll.x = node->view_target_scroll.x;
			}
			if (ABS(node->view_scroll.y - node->view_target_scroll.y) < 1)
			{
				node->view_scroll.y = node->view_target_scroll.y;
			}
		}
		else
		{
			node->view_scroll = node->view_target_scroll;
		}
	}
	else
	{
		node->dim = node->computed_dim;
		node->view_scroll = node->view_target_scroll;
		node->rect = node->computed_rect;
	}
	
	// TODO(fakhri): hot animations
	// TODO(fakhri): active animations
	
	b32 ishot    = g_ui->active_id == node->id;
	b32 is_hot = g_ui->hot_id == node->id;
	
	node->active_t    += (!!ishot - node->active_t) * step69;
	node->hot_t += (!!is_hot - node->hot_t) * step69;
	
	node->rect = range_center_dim(range_center(node->rect), node->dim);
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_animate_elements(child);
	}
}


internal void
ui_handle_this_frame_input(UI_Element *node)
{
	// NOTE(fakhri): Process the input at the end of the frame after we have built the 
	// hierarchy, this is done so that we can handle events in the correct order, when
	// child ui elements also process events (childs should consume events first)
	// This however introduces a 1-frame delay as the events from the current frame
	// won't be processed until the next frame... 
	
	// TODO(fakhri): is there a better way to do this without having
	// to introduce the 1-frame delay
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_handle_this_frame_input(child);
	}
	
	ui_update_element_interaction(node);
}

internal void
ui_modal_begin(String8 string)
{
	assert(!g_ui->modal_root);
	
	ui_push_layer(UI_Layer_Popup);
	UI_Element *modal_root = ui_element(string, UI_FLAG_Floating | UI_FLAG_Clip);
	g_ui->modal_root = modal_root;
	ui_push_parent(modal_root);
}

internal void
ui_modal_end()
{
	ui_pop_parent();
	ui_pop_layer();
}

internal void
ui_set_context(_Mplayer_UI *ui)
{
	g_ui = ui;
}

internal void
ui_init(Mplayer_Context *mplayer, Font *default_font)
{
	ui_set_context(&mplayer->ui);
	
	g_ui->input = &mplayer->input;
	g_ui->frame_arena = &mplayer->frame_arena;
	g_ui->arena       = &mplayer->main_arena;
	g_ui->def_font    = default_font;
}

internal void
ui_begin(Render_Group *group, V2_F32 mouse_p)
{
	g_ui->group   = group;
	g_ui->mouse_pos = mouse_p;
	g_ui->frame_index += 1;
	
	//-NOTE(fakhri): purge untouched elements from hashtable
	for (u32 i = 0; i < array_count(g_ui->elements_table); i += 1)
	{
		UI_Elements_Bucket *bucket = g_ui->elements_table + i;
		for (UI_Element *element = bucket->first, *next = 0; element; element = next)
		{
			next = element->next_hash;
			
			if (!element->id || element->frame_index + 1 < g_ui->frame_index)
			{
				DLLRemove_NP(bucket->first, bucket->last, element, next_hash, prev_hash);
				StackPush_N(g_ui->free_elements, element, next_free);
				if (g_ui->hot_id == element->id) g_ui->hot_id = 0;
				if (g_ui->active_id == element->id) g_ui->active_id = 0;
				if (g_ui->selected_id == element->id) g_ui->selected_id = 0;
			}
		}
	}
	
	g_ui->curr_parent = 0;
	
	ui_reset_default_style();
	ui_push_default_style();
	
	//ui_next_width(ui_size_pixel(group->render_ctx->draw_dim.width, 1));
	//ui_next_height(ui_size_pixel(group->render_ctx->draw_dim.height, 1));
	UI_Element *root = ui_element(str8_lit("root_ui_element"), 0);
	root->child_layout_axis = Axis2_Y;
	g_ui->root = root;
	ui_push_parent(root);
	
	g_ui->modal_root = 0;
}


internal void
ui_end()
{
	ui_pop_parent();
	
	if (g_ui->hot_id)
	{
		UI_Element *hot_element = ui_element_by_id(g_ui->hot_id);
		platform->set_cursor(hot_element->hover_cursor);
	}
	else
	{
		platform->set_cursor(Cursor_Arrow);
	}
	
	if (g_ui->root)
	{
		g_ui->root->size[Axis2_X] = ui_size_pixel(g_ui->group->render_ctx->draw_dim.x, 1);
		g_ui->root->size[Axis2_Y] = ui_size_pixel(g_ui->group->render_ctx->draw_dim.y, 1);
		g_ui->root->computed_rect = range_center_dim(vec2(0, 0), g_ui->root->computed_dim);
	}
	
	if (g_ui->modal_root)
	{
		g_ui->modal_root->computed_rect = range_center_dim(vec2(0, 0), g_ui->root->computed_dim);
	}
	
	ui_update_layout(g_ui->root);
	ui_animate_elements(g_ui->root);
	ui_handle_this_frame_input(g_ui->root);
	ui_draw_elements(g_ui->group, g_ui->root);
}
