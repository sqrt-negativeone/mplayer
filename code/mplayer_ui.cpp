
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
#define ui_parent(ui, p) _defer_loop(ui_push_parent(ui, p), ui_pop_parent(ui))
#define ui_vertical_layout(ui)   ui_parent(ui, ui_layout(ui, Axis2_Y))
#define ui_horizontal_layout(ui) ui_parent(ui, ui_layout(ui, Axis2_X))

#define ui_pref_size(ui, axis, size)                   _defer_loop(ui_push_size(ui, axis, size), ui_pop_size(ui))
#define ui_pref_width(ui, size)                        _defer_loop(ui_push_width(ui, size), ui_pop_width(ui))
#define ui_pref_height(ui, size)                       _defer_loop(ui_push_height(ui, size), ui_pop_height(ui))
#define ui_pref_background_color(ui, bg_color)         _defer_loop(ui_push_background_color(ui, bg_color), ui_pop_background_color(ui))
#define ui_pref_text_color(ui, text_color)             _defer_loop(ui_push_text_color(ui, text_color), ui_pop_text_color(ui))
#define ui_pref_border_color(ui, border_color)         _defer_loop(ui_push_border_color(ui, border_color), ui_pop_border_color(ui))
#define ui_pref_border_thickness(ui, border_thickness) _defer_loop(ui_push_border_thickness(ui, border_thickness), ui_pop_border_thickness(ui))
#define ui_pref_texture(ui, border_thickness)          _defer_loop(ui_push_texture(ui, texture), ui_pop_texture(ui))
#define ui_pref_font(ui, font)                         _defer_loop(ui_push_font(ui, font), ui_pop_font(ui))
#define ui_pref_texture_tint_color(ui, tint_color)     _defer_loop(ui_push_texture_tint_color(ui, tint_color), ui_pop_texture_tint_color(ui))
#define ui_pref_flags(ui, flags)     _defer_loop(ui_push_flags(ui, flags), ui_pop_flags(ui))
#define ui_pref_roundness(ui, roundness)   _defer_loop(ui_push_roundness(ui, roundness), ui_pop_roundness(ui))
#define ui_pref_scroll_step(ui, scroll_step)   _defer_loop(ui_push_scroll_step(ui, scroll_step), ui_pop_scroll_step(ui))
#define ui_pref_hover_cursor(ui, hover_cursor)   _defer_loop(ui_push_hover_cursor(ui, hover_cursor), ui_pop_hover_cursor(ui))
#define ui_pref_childs_axis(ui, axis)   _defer_loop(ui_push_childs_axis(ui, axis), ui_pop_childs_axis(ui))
#define ui_pref_layer(ui, layer)   _defer_loop(ui_push_layer(ui, layer), ui_pop_layer(ui))

#define ui_auto_pop_style(ui) do {              \
ui_auto_pop_stack(ui->fonts);               \
ui_auto_pop_stack(ui->textures);            \
ui_auto_pop_stack(ui->text_colors);         \
ui_auto_pop_stack(ui->texture_tint_colors); \
ui_auto_pop_stack(ui->bg_colors);           \
ui_auto_pop_stack(ui->border_colors);       \
ui_auto_pop_stack(ui->border_thickness);    \
ui_auto_pop_stack(ui->sizes[Axis2_X]);      \
ui_auto_pop_stack(ui->sizes[Axis2_Y]);      \
ui_auto_pop_stack(ui->flags_stack);         \
ui_auto_pop_stack(ui->roundness_stack);     \
ui_auto_pop_stack(ui->scroll_steps);        \
ui_auto_pop_stack(ui->hover_cursors);       \
ui_auto_pop_stack(ui->childs_axis);         \
ui_auto_pop_stack(ui->layers);         \
} while (0)

//- NOTE(fakhri): background color stack
internal void
ui_push_background_color(Mplayer_UI *ui, V4_F32 background_color)
{
	ui_stack_push(ui->bg_colors, background_color);
}
internal void
ui_next_background_color(Mplayer_UI *ui, V4_F32 background_color)
{
	ui_stack_push_auto_pop(ui->bg_colors, background_color);
}

internal void
ui_pop_background_color(Mplayer_UI *ui)
{
	ui_stack_pop(ui->bg_colors);
}

// NOTE(fakhri): Size Stack
internal void
ui_push_size(Mplayer_UI *ui, Axis2 axis, UI_Size size)
{
	ui_stack_push(ui->sizes[axis], size);
}

internal void
ui_next_size(Mplayer_UI *ui, Axis2 axis, UI_Size size)
{
	ui_stack_push_auto_pop(ui->sizes[axis], size);
}

internal void
ui_pop_size(Mplayer_UI *ui, Axis2 axis)
{
	ui_stack_pop(ui->sizes[axis]);
}

internal void
ui_push_width(Mplayer_UI *ui, UI_Size size)
{
	ui_push_size(ui, Axis2_X, size);
}

internal void
ui_next_width(Mplayer_UI *ui, UI_Size size)
{
	ui_next_size(ui, Axis2_X, size);
}

internal void
ui_push_height(Mplayer_UI *ui, UI_Size size)
{
	ui_push_size(ui, Axis2_Y, size);
}

internal void
ui_next_height(Mplayer_UI *ui, UI_Size size)
{
	ui_next_size(ui, Axis2_Y, size);
}

internal void
ui_pop_width(Mplayer_UI *ui)
{
	ui_pop_size(ui, Axis2_X);
}

internal void
ui_pop_height(Mplayer_UI *ui)
{
	ui_pop_size(ui, Axis2_Y);
}

//- NOTE(fakhri): text color stack
internal void
ui_push_text_color(Mplayer_UI *ui, V4_F32 text_color)
{
	ui_stack_push(ui->text_colors, text_color);
}
internal void
ui_next_text_color(Mplayer_UI *ui, V4_F32 text_color)
{
	ui_stack_push_auto_pop(ui->text_colors, text_color);
}

internal void
ui_pop_text_color(Mplayer_UI *ui)
{
	ui_stack_pop(ui->text_colors);
}

//- NOTE(fakhri): border color stack
internal void
ui_push_border_color(Mplayer_UI *ui, V4_F32 border_color)
{
	ui_stack_push(ui->border_colors, border_color);
}
internal void
ui_next_border_color(Mplayer_UI *ui, V4_F32 border_color)
{
	ui_stack_push_auto_pop(ui->border_colors, border_color);
}
internal void
ui_pop_border_color(Mplayer_UI *ui)
{
	ui_stack_pop(ui->border_colors);
}

//- NOTE(fakhri): border thickness stack
internal void
ui_push_border_thickness(Mplayer_UI *ui, f32 border_thickness)
{
	ui_stack_push(ui->border_thickness, border_thickness);
}
internal void
ui_next_border_thickness(Mplayer_UI *ui, f32 border_thickness)
{
	ui_stack_push_auto_pop(ui->border_thickness, border_thickness);
}
internal void
ui_pop_border_thickness(Mplayer_UI *ui)
{
	ui_stack_pop(ui->border_thickness);
}

//- NOTE(fakhri): texture stack
internal void
ui_push_texture(Mplayer_UI *ui, Texture texture)
{
	ui_stack_push(ui->textures, texture);
}
internal void
ui_next_texture(Mplayer_UI *ui, Texture texture)
{
	ui_stack_push_auto_pop(ui->textures, texture);
}
internal void
ui_pop_texture(Mplayer_UI *ui)
{
	ui_stack_pop(ui->textures);
}

//- NOTE(fakhri): font stack
internal void
ui_push_font(Mplayer_UI *ui, Mplayer_Font *font)
{
	ui_stack_push(ui->fonts, font);
}
internal void
ui_next_font(Mplayer_UI *ui, Mplayer_Font *font)
{
	ui_stack_push_auto_pop(ui->fonts, font);
}
internal void
ui_pop_font(Mplayer_UI *ui)
{
	ui_stack_pop(ui->fonts);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_texture_tint_color(Mplayer_UI *ui, V4_F32 tint_color)
{
	ui_stack_push(ui->texture_tint_colors, tint_color);
}
internal void
ui_next_texture_tint_color(Mplayer_UI *ui, V4_F32 tint_color)
{
	ui_stack_push_auto_pop(ui->texture_tint_colors, tint_color);
}
internal void
ui_pop_texture_tint_color(Mplayer_UI *ui)
{
	ui_stack_pop(ui->texture_tint_colors);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_flags(Mplayer_UI *ui, u32 flags)
{
	ui_stack_push(ui->flags_stack, flags);
}
internal void
ui_next_flags(Mplayer_UI *ui, u32 flags)
{
	ui_stack_push_auto_pop(ui->flags_stack, flags);
}
internal void
ui_pop_flags(Mplayer_UI *ui)
{
	ui_stack_pop(ui->flags_stack);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_roundness(Mplayer_UI *ui, f32 roundness)
{
	ui_stack_push(ui->roundness_stack, roundness);
}
internal void
ui_next_roundness(Mplayer_UI *ui, f32 roundness)
{
	ui_stack_push_auto_pop(ui->roundness_stack, roundness);
}
internal void
ui_pop_roundness(Mplayer_UI *ui)
{
	ui_stack_pop(ui->roundness_stack);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_scroll_step(Mplayer_UI *ui, V2_F32 scroll_step)
{
	ui_stack_push(ui->scroll_steps, scroll_step);
}
internal void
ui_next_scroll_step(Mplayer_UI *ui, V2_F32 scroll_step)
{
	ui_stack_push_auto_pop(ui->scroll_steps, scroll_step);
}
internal void
ui_pop_scroll_step(Mplayer_UI *ui)
{
	ui_stack_pop(ui->scroll_steps);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_hover_cursor(Mplayer_UI *ui, u32 hover_cursor)
{
	ui_stack_push(ui->hover_cursors, hover_cursor);
}
internal void
ui_next_hover_cursor(Mplayer_UI *ui, u32 hover_cursor)
{
	ui_stack_push_auto_pop(ui->hover_cursors, hover_cursor);
}
internal void
ui_pop_hover_cursor(Mplayer_UI *ui)
{
	ui_stack_pop(ui->hover_cursors);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_childs_axis(Mplayer_UI *ui, u32 axis)
{
	ui_stack_push(ui->childs_axis, axis);
}
internal void
ui_next_childs_axis(Mplayer_UI *ui, u32 axis)
{
	ui_stack_push_auto_pop(ui->childs_axis, axis);
}
internal void
ui_pop_childs_axis(Mplayer_UI *ui)
{
	ui_stack_pop(ui->childs_axis);
}

//- NOTE(fakhri): texture tint color stack
internal void
ui_push_layer(Mplayer_UI *ui, u32 layer)
{
	ui_stack_push(ui->layers, layer);
}
internal void
ui_next_layer(Mplayer_UI *ui, u32 layer)
{
	ui_stack_push_auto_pop(ui->layers, layer);
}
internal void
ui_pop_layer(Mplayer_UI *ui)
{
	ui_stack_pop(ui->layers);
}

internal void
ui_push_default_style(Mplayer_UI *ui)
{
	ui_push_font(ui, ui->def_font);
	ui_push_texture(ui, NULL_TEXTURE);
	ui_push_text_color(ui, vec4(1));
	ui_push_texture_tint_color(ui, vec4(1));
	ui_push_background_color(ui, vec4(0));
	ui_push_border_color(ui, vec4(0,0,0,1));
	ui_push_border_thickness(ui, 0);
	ui_push_roundness(ui, 0);
	ui_push_flags(ui, 0);
	ui_push_scroll_step(ui, vec2(0, -50));
	ui_push_hover_cursor(ui, Cursor_Arrow);
	ui_push_childs_axis(ui, Axis2_Y);
	ui_push_layer(ui, UI_Layer_Default);
	ui_push_width(ui, ui_size_parent_remaining());
	ui_push_height(ui, ui_size_parent_remaining());
}

internal void
ui_reset_default_style(Mplayer_UI *ui)
{
	ui->fonts.top = 0;
	ui->textures.top = 0;
	ui->text_colors.top = 0;
	ui->texture_tint_colors.top = 0;
	ui->bg_colors.top = 0;
	ui->border_colors.top = 0;
	ui->border_thickness.top = 0;
	ui->roundness_stack.top = 0;
	ui->flags_stack.top = 0;
	ui->scroll_steps.top = 0;
	ui->hover_cursors.top = 0;
	ui->childs_axis.top = 0;
	ui->layers.top = 0;
	ui->sizes[Axis2_X].top = 0;
	ui->sizes[Axis2_Y].top = 0;
}


internal void
ui_update_element_interaction(Mplayer_UI *ui, UI_Element *element)
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
	
	if (ui->popup_root)
	{
		b32 in_popup = 0;
		for (UI_Element *p = element->parent; p; p = p->parent)
		{
			if (p == ui->popup_root)
			{
				in_popup = 1;
				break;
			}
		}
		
		if (!in_popup)
		{
			ignore = 1;
		}
	}
	
	V2_F32 mouse_p = ui->mouse_pos;
	b32 mouse_over = is_in_range(interaction_rect, mouse_p) && !ignore;
	
	// TODO(fakhri): Bug: 
	// - click an element
	// - move the mouse over a child of that element
	// - release the mouse, the parent element stays hot and is not reset.
	// 
	// If we click somewhere empty and drag the mouse over the element
	// and release we get a click event. this should not happen.
	
	for (Mplayer_Input_Event *e = ui->input->first_event, *next = 0; e; e = next)
	{
		next = e->next;
		b32 consumed = 0;
		if (has_flag(element->flags, UI_FLAG_Left_Mouse_Clickable) && e->kind == Event_Kind_Mouse_Move)
		{
			if (mouse_over)
			{
				consumed = 1;
				ui->active_id = id;
			}
			else if (ui->active_id == id)
			{
				consumed = 0;
				ui->active_id = 0;
			}
		}
		
		if (has_flag(element->flags, UI_FLAG_Left_Mouse_Clickable) && e->kind == Event_Kind_Mouse_Key && e->key == Mouse_Left)
		{
			if (mouse_over && e->is_down)
			{
				consumed = 1;
				ui->hot_id = id;
				ui->active_id = id;
				ui->mouse_drag_start_pos = mouse_p;
				ui->recent_click_time = ui->input->time;
				set_flag(interaction.flags, UI_Interaction_Pressed);
			}
			if (!e->is_down)
			{
				if (ui->active_id == id)
				{
					consumed = 1;
					if (ui->hot_id == id)
						set_flag(interaction.flags, UI_Interaction_Clicked);
					ui->active_id = 0;
				}
				
				if (ui->hot_id == id)
				{
					consumed = 1;
					set_flag(interaction.flags, UI_Interaction_Released);
					ui->hot_id = 0;
				}
				
				if (has_flag(element->flags, UI_FLAG_Selectable))
				{
					if (!mouse_over && ui->selected_id == id) ui->selected_id = 0;
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
			DLLRemove(ui->input->first_event, ui->input->last_event, e);
		}
	}
	
	if (mouse_over)
	{
		set_flag(interaction.flags, UI_Interaction_Hover);
	}
	else
	{
		if (ui->active_id == id)
		{
			ui->active_id = 0;
		}
	}
	
	if (ui->hot_id == id)
	{
		set_flag(interaction.flags, UI_Interaction_Pressed);
	}
	
	if (has_flag(element->flags, UI_FLAG_Selectable))
	{
		if (interaction.clicked)
		{
			ui->selected_id = id;
		}
		if (!mouse_over && interaction.released && ui->selected_id == id)
		{
			ui->selected_id = 0;
		}
		
		if (ui->selected_id == id)
		{
			set_flag(interaction.flags, UI_Interaction_Selected);
		}
	}
	
	element->last_frame_interaction = interaction;
}

internal Mplayer_UI_Interaction
ui_interaction_from_element(Mplayer_UI *ui, UI_Element *element)
{
	Mplayer_UI_Interaction interaction = element->last_frame_interaction;
	return interaction;
}

internal void
ui_push_parent(Mplayer_UI *ui, UI_Element *node)
{
	ui->curr_parent = node;
}

internal void
ui_pop_parent(Mplayer_UI *ui)
{
	UI_Element *parent = ui->curr_parent;
	if (parent)
	{
		ui->curr_parent = parent->parent;
	}
}

// NOTE(fakhri): from http://www.cse.yorku.ca/~oz/hash.html
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
ui_element_by_id(Mplayer_UI *ui, u64 id)
{
	UI_Element *result = 0;
	u32 bucket_index = id % array_count(ui->elements_table);
	if (id)
	{
		for (UI_Element *ht_node = ui->elements_table[bucket_index].first; ht_node; ht_node = ht_node->next_hash)
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
ui_element(Mplayer_UI *ui, String8 string, UI_Element_Flags flags = 0)
{
	UI_Element *result = 0;
	
	//- NOTE(fakhri): check hash table
	String8 key = ui_key_from_string(string);
	u64 id = ui_hash_key(key);
	u32 bucket_index = id % array_count(ui->elements_table);
	if (id)
	{
		for (UI_Element *ht_node = ui->elements_table[bucket_index].first; ht_node; ht_node = ht_node->next_hash)
		{
			if (ht_node->id == id)
			{
				assert(ht_node->frame_index < ui->frame_index);
				result = ht_node;
				break;
			}
		}
	}
	
	if (!result)
	{
		//- NOTE(fakhri): create new element
		result = ui->free_elements;
		if (result)
		{
			ui->free_elements = result->next_free;
			memory_zero_struct(result);
		}
		else
		{
			result = m_arena_push_struct_z(ui->arena, UI_Element);
		}
		
		// NOTE(fakhri): push to the hash table
		DLLPushBack_NP(ui->elements_table[bucket_index].first, ui->elements_table[bucket_index].last, result, next_hash, prev_hash);
	}
	
	//- NOTE(fakhri): init ui element
	assert(result);
	result->flags = flags;
	result->size[0]  = ui_stack_top(ui->sizes[0]);
	result->size[1]  = ui_stack_top(ui->sizes[1]);
	result->id = id;
	result->frame_index = ui->frame_index;
	result->parent = result->next = result->prev = result->first = result->last = 0;
	
	result->layer = (UI_Layer)ui_stack_top(ui->layers);
	result->hover_cursor = (Cursor_Shape)ui_stack_top(ui->hover_cursors);
	result->child_layout_axis = (Axis2)ui_stack_top(ui->childs_axis);
	result->flags |= ui_stack_top(ui->flags_stack);
	result->roundness = ui_stack_top(ui->roundness_stack);
	
	if (result->last_frame_interaction.element != result)
	{
		result->last_frame_interaction = ZERO_STRUCT;
		result->last_frame_interaction.element = result;
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Background))
	{
		result->background_color = ui_stack_top(ui->bg_colors);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Border))
	{
		result->border_color     = ui_stack_top(ui->border_colors);
		result->border_thickness = ui_stack_top(ui->border_thickness);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Image))
	{
		result->texture = ui_stack_top(ui->textures);
		result->texture_tint_color = ui_stack_top(ui->texture_tint_colors);
	}
	
	if (has_flag(flags, UI_FLAG_Draw_Text))
	{
		result->font       = ui_stack_top(ui->fonts);
		result->text_color = ui_stack_top(ui->text_colors);
		result->text       = ui_drawable_text_from_string(string);
	}
	
	if (has_flag(flags, UI_FLAG_View_Scroll))
	{
		result->scroll_step = ui_stack_top(ui->scroll_steps);
	}
	
	if (ui->curr_parent)
	{
		DLLPushBack(ui->curr_parent->first, ui->curr_parent->last, result);
		result->parent = ui->curr_parent;
	}
	
	// ui_element_push(ui, result);
	
	ui_auto_pop_style(ui);
	return result;
}

internal UI_Element *
ui_element_f(Mplayer_UI *ui, UI_Element_Flags flags, const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(ui->frame_arena, fmt, args);
	va_end(args);
	
	UI_Element *result = ui_element(ui, string, flags);
	return result;
}


//- NOTE(fakhri): layout
internal UI_Element *
ui_layout(Mplayer_UI *ui, Axis2 child_layout_axis)
{
	ui_next_childs_axis(ui, child_layout_axis);
	UI_Element *layout = ui_element(ui, str8_lit(""), UI_FLAG_Draw_Background | UI_FLAG_Draw_Border);
	return layout;
}

internal void
ui_spacer(Mplayer_UI *ui, UI_Size size)
{
	UI_Element *parent = ui->curr_parent;
	if (parent)
	{
		ui_next_size(ui, parent->child_layout_axis, size);
		ui_next_size(ui, inverse_axis(parent->child_layout_axis), ui_size_pixel(0, 0));
		UI_Element *spacer = ui_element(ui, str8_lit(""), 0);
	}
}

#define ui_spacer_pixels(ui, px, strictness) ui_spacer(ui, ui_size_pixel(px, strictness))
#define ui_padding(ui, size) _defer_loop(ui_spacer(ui, size), ui_spacer(ui, size))

internal UI_Element *
ui_label(Mplayer_UI *ui, String8 string)
{
	UI_Element *label = ui_element(ui, ZERO_STRUCT,
		UI_FLAG_Draw_Text |
		UI_FLAG_Draw_Border |
		UI_FLAG_Draw_Background
	);
	
	label->text = str8_clone(ui->frame_arena, ui_drawable_text_from_string(string));
	return label;
}

internal Mplayer_UI_Interaction
ui_button(Mplayer_UI *ui, String8 string)
{
	ui_next_hover_cursor(ui, Cursor_Hand);
	UI_Element *button = ui_element(ui, string,
		UI_FLAG_Clickable |
		UI_FLAG_Draw_Background |
		UI_FLAG_Draw_Text
	);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, button);
	
	return interaction;
}

internal Mplayer_UI_Interaction
ui_button_f(Mplayer_UI *ui, const char *fmt, ...)
{
	va_list args;
  va_start(args, fmt);
	String8 string = str8_fv(ui->frame_arena, fmt, args);
	va_end(args);
	
	Mplayer_UI_Interaction interaction = ui_button(ui, string);
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
	if (ui->selected_id == element->id)
	{
		V2_F32 pos = range_center(element->rect);
		f32 blink_t = (sin_f(2 * PI32 * (ui->input->time - ui->recent_click_time)) + 1) / 2;
		if (ui->input->time - ui->recent_click_time > 5)
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
ui_input_field(Mplayer_UI *ui, String8 key, String8 *buffer, u64 max_capacity)
{
	b32 edited = 0;
	ui_next_childs_axis(ui, Axis2_Y);
	ui_next_hover_cursor(ui, Cursor_TextSelect);
	UI_Element *text_input = ui_element(ui, key, 
		UI_FLAG_Draw_Background|UI_FLAG_Draw_Border|UI_FLAG_Clickable|UI_FLAG_Selectable);
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, text_input);
	if (interaction.hover)
	{
		text_input->background_color = vec4(0.3f, 0.3f, 0.3f, 1);
	}
	
	if (interaction.selected)
	{
		text_input->background_color = vec4(0.35f, 0.35f, 0.35f, 1);
		
		Mplayer_Font *font = ui_stack_top(ui->fonts);
		V2_F32 text_dim = font_compute_text_dim(font, *buffer);
		if (interaction.clicked || interaction.pressed)
		{
			V2_F32 pos = range_center(text_input->rect);
			f32 test_x = pos.x - 0.5f * text_dim.width;
			ui->input_cursor = 0;
			for (u32 i = 0; i < buffer->len; i += 1)
			{
				f32 glyph_width = font_get_glyph_from_char(font, buffer->str[i]).advance;
				if (test_x + 0.5f * glyph_width < ui->mouse_pos.x)
				{
					ui->input_cursor = i + 1;
				}
				else break;
				
				test_x += glyph_width;
			}
		}
		
		for (Mplayer_Input_Event *event = ui->input->first_event; event; event = event->next)
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
						if (ui->input_cursor < buffer->len)
						{
							memory_move(buffer->str + ui->input_cursor + 1,
								buffer->str + ui->input_cursor,
								buffer->len - ui->input_cursor);
						}
						
						buffer->str[ui->input_cursor] = (u8)event->text_character;
						buffer->len += 1;
						ui->input_cursor += 1;
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
							if (ui->input_cursor && buffer->len)
							{
								memory_move(buffer->str + ui->input_cursor - 1,
									buffer->str + ui->input_cursor,
									buffer->len - ui->input_cursor);
								buffer->len     -= 1;
								ui->input_cursor -= 1;
							}
						}
						else if (event->key == Keyboard_Left)
						{
							consumed = true;
							if (ui->input_cursor)
								ui->input_cursor -= 1;
						}
						else if (event->key == Keyboard_Right)
						{
							consumed = true;
							if (ui->input_cursor < buffer->len)
								ui->input_cursor += 1;
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
				DLLRemove(ui->input->first_event, ui->input->last_event, event);
			}
		}
		if (ui->input_cursor > buffer->len)
		{
			ui->input_cursor = (u32)buffer->len;
		}
		
		
		UI_Input_Field_Draw_Data *draw_data = m_arena_push_struct_z(ui->frame_arena, UI_Input_Field_Draw_Data);
		draw_data->cursor_offset_x = -0.5f * text_dim.width + font_compute_text_dim(font, str8(buffer->str, MIN(buffer->len, ui->input_cursor))).width;
		draw_data->cursor_dim = vec2(2.1f, 1.25f * text_dim.height);
		ui_element_set_draw_proc(text_input, ui_input_field_default_draw, draw_data);
	}
	
	ui_parent(ui, text_input) ui_padding(ui, ui_size_parent_remaining())
	{
		ui_next_height(ui, ui_size_text_dim(1));
		UI_Element *text = ui_label(ui, *buffer);
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
	slider_dim.height = lerp(element->dim.height, element->active_t, 1.3f * element->dim.height);
	slider_dim.height = lerp(slider_dim.height, element->hot_t, 0.9f * element->dim.height);
	
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
ui_slider_f32(Mplayer_UI *ui, String8 string, f32 *val, f32 min, f32 max)
{
	ui_next_hover_cursor(ui, Cursor_Hand);
	UI_Element *slider = ui_element(ui, string, UI_FLAG_Draw_Background | UI_FLAG_Clickable);
	f32 percent = map_into_range_zo(min, *val, max);
	
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, slider);
	if (interaction.pressed)
	{
		percent = map_into_range_zo(slider->computed_rect.min_x, 
			ui->mouse_pos.x,
			slider->computed_rect.max_x);
	}
	
	UI_Slider_Draw_Data *slider_data = m_arena_push_struct_z(ui->frame_arena, UI_Slider_Draw_Data);
	slider_data->percent = percent;
	ui_element_set_draw_proc(slider, ui_slider_default_draw_proc, slider_data);
	
	*val = map_into_range(percent, 0, 1, min, max);
	return interaction;
}

internal Mplayer_UI_Interaction
ui_slider_u64(Mplayer_UI *ui, String8 string, u64 *val, u64 min, u64 max)
{
	ui_next_hover_cursor(ui, Cursor_Hand);
	UI_Element *slider = ui_element(ui, string, UI_FLAG_Draw_Background | UI_FLAG_Clickable);
	f32 percent = (f32)map_into_range_zo((f64)min, (f64)*val, (f64)max);
	
	Mplayer_UI_Interaction interaction = ui_interaction_from_element(ui, slider);
	if (interaction.pressed)
	{
		percent = map_into_range_zo(slider->computed_rect.min_x, 
			ui->mouse_pos.x,
			slider->computed_rect.max_x);
	}
	
	UI_Slider_Draw_Data *slider_data = m_arena_push_struct_z(ui->frame_arena, UI_Slider_Draw_Data);
	slider_data->percent = percent;
	ui_element_set_draw_proc(slider, ui_slider_default_draw_proc, slider_data);
	
	*val = u64(min + percent * (max - min));
	*val = CLAMP(min, *val, max);
	return interaction;
}


internal void
ui_grid_push_row(Mplayer_UI *ui)
{
	ui_next_height(ui, ui_size_pixel(ui->grid_item_dim.height, 1));
	ui_push_parent(ui, ui_layout(ui, Axis2_X));
	ui_spacer(ui, ui_size_parent_remaining());
	ui->grid_item_index_in_row = 0;
}

internal b32
ui_grid_item_begin(Mplayer_UI *ui)
{
	b32 good = 0;
	if (ui->grid_item_index < ui->grid_items_count && 
		ui->grid_item_index_in_row < ui->grid_item_count_per_row)
	{
		good = 1;
		
		ui->grid_item_index += 1;
		ui->grid_item_index_in_row += 1;
		ui_next_width(ui, ui_size_pixel(ui->grid_item_dim.width, 1));
		ui_next_height(ui, ui_size_pixel(ui->grid_item_dim.height, 1));
		UI_Element *grid_item = ui_element(ui, {0}, 0);
		ui_push_parent(ui, grid_item);
	}
	return good;
}


internal void
ui_grid_item_end(Mplayer_UI *ui)
{
	// NOTE(fakhri): pop grid item
	ui_pop_parent(ui);
	ui_spacer(ui, ui_size_parent_remaining());
	
	// NOTE(fakhri): check if row is full
	if (ui->grid_item_index_in_row == ui->grid_item_count_per_row)
	{
		ui_pop_parent(ui); // pop parent row
		ui_spacer_pixels(ui, ui->grid_vpadding, 1);
		ui->grid_row_index += 1;
		
		if (ui->grid_row_index < (i32)ui->grid_visible_rows_count)
		{
			ui_grid_push_row(ui);
		}
	}
}

internal u32
ui_grid_begin(Mplayer_UI *ui, String8 string, u32 items_count, V2_F32 grid_item_dim, f32 vpadding)
{
	ui_next_childs_axis(ui, Axis2_Y);
	UI_Element *grid = ui_element(ui, string, UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	ui_push_parent(ui, grid);
	
	u32 item_count_per_row = (u32)((grid->computed_dim.width) / grid_item_dim.width);
	item_count_per_row = MAX(item_count_per_row, 1);
	
	u32 max_rows_count = ((items_count + item_count_per_row - 1) / item_count_per_row);
	
	u32 visible_rows_count = u32(grid->computed_dim.height / (grid_item_dim.height + vpadding)) + 2;
	
	f32 space_before_first_row = ABS(grid->view_scroll.y);
	u32 first_visible_row = (u32)(space_before_first_row / (grid_item_dim.height + vpadding));
	first_visible_row = MIN(first_visible_row, max_rows_count-1);
	
	ui_spacer_pixels(ui, first_visible_row * (grid_item_dim.height + vpadding), 1);
	
	ui->grid_items_count = items_count;
	ui->grid_first_visible_row = first_visible_row;
	ui->grid_item_count_per_row = item_count_per_row;
	ui->grid_max_rows_count = max_rows_count;
	ui->grid_item_index_in_row = 0;
	ui->grid_row_index = 0;
	ui->grid_visible_rows_count = visible_rows_count;
	ui->grid_vpadding = vpadding;
	ui->grid_item_dim = grid_item_dim;
	
	if (items_count)
	{
		ui_grid_push_row(ui);
	}
	
	ui->grid_item_index = first_visible_row * item_count_per_row;
	return ui->grid_item_index;
}

internal void
ui_grid_end(Mplayer_UI *ui)
{
	// NOTE(fakhri): pop incomplete rows
	if (ui->grid_row_index < (i32)ui->grid_visible_rows_count && 
		ui->grid_item_index_in_row < ui->grid_item_count_per_row)
	{
		ui_pop_parent(ui); // pop parent row
	}
	
	u32 rows_remaining = ui->grid_max_rows_count - MIN(ui->grid_first_visible_row + ui->grid_visible_rows_count, ui->grid_max_rows_count);
	ui_spacer_pixels(ui, rows_remaining * (ui->grid_item_dim.height + ui->grid_vpadding), 1);
	
	// NOTE(fakhri): pop grid
	ui_pop_parent(ui);
}

#define ui_for_each_grid_item(ui, string, items_count, item_dim, vpadding, item_index) \
defer(ui_grid_end(ui)) for (u32 item_index = ui_grid_begin(ui, string, items_count, item_dim, vpadding); \
ui_grid_item_begin(ui);\
(item_index += 1, ui_grid_item_end(ui)))

#define ui_for_each_list_item(ui, string, items_count, item_height, vpadding, item_index) \
defer(ui_list_end(ui)) for (u32 item_index = ui_list_begin(ui, string, items_count, item_height, vpadding); \
ui_list_item_begin(ui);\
(item_index += 1, ui_list_item_end(ui)))


internal b32
ui_list_item_begin(Mplayer_UI *ui)
{
	b32 good = 0;
	if (ui->list_item_index < ui->list_items_count && 
		ui->list_row_index < ui->list_visible_rows_count)
	{
		good = 1;
		
		ui->list_item_index += 1;
		ui->list_row_index += 1;
		
		ui_next_width(ui, ui_size_parent_remaining());
		ui_next_height(ui, ui_size_pixel(ui->list_item_height, 1));
		UI_Element *list_item = ui_element(ui, {0}, 0);
		
		ui_push_parent(ui, list_item);
	}
	return good;
}

internal void
ui_list_item_end(Mplayer_UI *ui)
{
	ui_pop_parent(ui);
	ui_spacer_pixels(ui, ui->list_vpadding, 1);
}

internal u32
ui_list_begin(Mplayer_UI *ui, String8 string, u32 items_count, f32 item_height, f32 vpadding)
{
	UI_Element *list = ui_element(ui, string, UI_FLAG_Draw_Border | UI_FLAG_Draw_Background | UI_FLAG_View_Scroll | UI_FLAG_OverflowY | UI_FLAG_Animate_Scroll | UI_FLAG_Clip);
	list->child_layout_axis = Axis2_Y;
	ui_push_parent(ui, list);
	
	u32 visible_rows_count = u32(list->computed_dim.height / (item_height + vpadding)) + 2;
	
	f32 space_before_first_row = ABS(list->view_scroll.y);
	u32 first_visible_row = (u32)(space_before_first_row / (item_height + vpadding));
	first_visible_row = MIN(first_visible_row, items_count);
	
	ui_spacer_pixels(ui, first_visible_row * (item_height + vpadding), 1);
	
	ui->list_items_count = items_count;
	ui->list_first_visible_row = first_visible_row;
	ui->list_max_rows_count = items_count;
	ui->list_row_index = 0;
	ui->list_visible_rows_count = visible_rows_count;
	ui->list_vpadding = vpadding;
	ui->list_item_height = item_height;
	
	ui->list_item_index = first_visible_row;
	return ui->list_item_index;
}

internal void
ui_list_end(Mplayer_UI *ui)
{
	u32 rows_remaining = ui->list_max_rows_count - MIN(ui->list_first_visible_row + ui->list_visible_rows_count, ui->list_max_rows_count);
	ui_spacer_pixels(ui, rows_remaining * (ui->list_item_height + ui->list_vpadding), 1);
	
	// NOTE(fakhri): pop list
	ui_pop_parent(ui);
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
			node->computed_dim.v[axis] = font_compute_text_dim(node->font, node->text).v[axis];
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
ui_update_layout(Mplayer_UI *ui, UI_Element *root)
{
	for (u32 axis = Axis2_X; axis < Axis2_COUNT; axis += 1)
	{
		ui_layout_compute_preorder_sizes(root, (Axis2)axis);
		ui_layout_compute_postorder_sizes(root, (Axis2)axis);
		ui_layout_fix_sizes_violations(root, (Axis2)axis);
	}
}

internal void
ui_draw_elements(Mplayer_UI *ui, UI_Element *node)
{
	Range2_F32 old_cull = ui->group->config.cull_range;
	
	if (has_flag(node->flags, UI_FLAG_Clip))
	{
		render_group_add_cull_range(ui->group, node->rect);
	}
	
	V2_F32 pos = range_center(node->rect);
	// NOTE(fakhri): only render the leaf nodes
	if (has_flag(node->flags, UI_FLAG_Draw_Background))
	{
		push_rect(ui->group, pos, (f32)node->layer, node->dim, node->background_color, node->roundness);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Image))
	{
		push_image(ui->group, pos, (f32)node->layer, node->dim, node->texture, node->texture_tint_color, node->roundness);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Text))
	{
		draw_text(ui->group, node->font, pos, (f32)node->layer, node->text_color, fancy_str8(node->text), Text_Render_Flag_Centered);
	}
	
	if (has_flag(node->flags, UI_FLAG_Has_Custom_Draw))
	{
		node->custom_draw_proc(ui, ui->group, node);
	}
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_draw_elements(ui, child);
	}
	
	if (has_flag(node->flags, UI_FLAG_Draw_Border))
	{
		draw_outline(ui->group, pos, (f32)node->layer, node->dim, node->border_color, node->border_thickness);
	}
	
	
	if (has_flag(node->flags, UI_FLAG_Clip))
	{
		render_group_set_cull_range(ui->group, old_cull);
	}
	
}

internal void
ui_cache_or_dispose_hierarchy(Mplayer_UI *ui, UI_Element *node)
{
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_cache_or_dispose_hierarchy(ui, child);
	}
	
	// NOTE(fakhri): dispose of node
	node->next_free = ui->free_elements;
	ui->free_elements = node;
}

internal void
ui_animate_elements(Mplayer_UI *ui, UI_Element *node)
{
	f32 dt = ui->input->frame_dt;
	
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
	
	// TODO(fakhri): active animations
	// TODO(fakhri): hot animations
	
	b32 is_hot    = ui->hot_id == node->id;
	b32 is_active = ui->active_id == node->id;
	
	node->hot_t    += (!!is_hot - node->hot_t) * step69;
	node->active_t += (!!is_active - node->active_t) * step69;
	
	node->rect = range_center_dim(range_center(node->rect), node->dim);
	
	for (UI_Element *child = node->first; child; child = child->next)
	{
		ui_animate_elements(ui, child);
	}
}


internal void
ui_handle_this_frame_input(Mplayer_UI *ui, UI_Element *node)
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
		ui_handle_this_frame_input(ui, child);
	}
	
	ui_update_element_interaction(ui, node);
}

internal void
ui_popup_begin(Mplayer_UI *ui, String8 string)
{
	assert(!ui->popup_root);
	
	ui_push_layer(ui, UI_Layer_Popup);
	UI_Element *popup_root = ui_element(ui, string, UI_FLAG_Floating | UI_FLAG_Clip);
	ui->popup_root = popup_root;
	ui_push_parent(ui, popup_root);
}

internal void
ui_popup_end(Mplayer_UI *ui)
{
	ui_pop_parent(ui);
	ui_pop_layer(ui);
}

internal void
ui_begin(Mplayer_UI *ui, Mplayer_Context *mplayer, Render_Group *group, V2_F32 mouse_p)
{
	ui->group   = group;
	ui->mouse_pos = mouse_p;
	ui->disable_input = false;
	
	ui->input       = &mplayer->input;
	ui->frame_arena = &mplayer->frame_arena;
	ui->arena       = &mplayer->main_arena;
	ui->def_font    = &mplayer->font;
	ui->frame_index += 1;
	
	//-NOTE(fakhri): purge untouched elements from hashtable
	for (u32 i = 0; i < array_count(ui->elements_table); i += 1)
	{
		UI_Elements_Bucket *bucket = ui->elements_table + i;
		for (UI_Element *element = bucket->first, *next = 0; element; element = next)
		{
			next = element->next_hash;
			
			if (!element->id || element->frame_index + 1 < ui->frame_index)
			{
				DLLRemove_NP(bucket->first, bucket->last, element, next_hash, prev_hash);
				StackPush_N(ui->free_elements, element, next_free);
				if (ui->active_id == element->id) ui->active_id = 0;
				if (ui->hot_id == element->id) ui->hot_id = 0;
			}
		}
	}
	
	ui->curr_parent = 0;
	
	ui_reset_default_style(ui);
	ui_push_default_style(ui);
	
	//ui_next_width(ui, ui_size_pixel(group->render_ctx->draw_dim.width, 1));
	//ui_next_height(ui, ui_size_pixel(group->render_ctx->draw_dim.height, 1));
	UI_Element *root = ui_element(ui, str8_lit("root_ui_element"), 0);
	root->child_layout_axis = Axis2_Y;
	ui->root = root;
	ui_push_parent(ui, root);
	
	ui->popup_root = 0;
}


internal void
ui_end(Mplayer_UI *ui)
{
	ui_pop_parent(ui);
	
	if (ui->active_id)
	{
		UI_Element *active_element = ui_element_by_id(ui, ui->active_id);
		platform->set_cursor(active_element->hover_cursor);
	}
	else
	{
		platform->set_cursor(Cursor_Arrow);
	}
	
	if (ui->root)
	{
		ui->root->size[Axis2_X] = ui_size_pixel(ui->group->render_ctx->draw_dim.x, 1);
		ui->root->size[Axis2_Y] = ui_size_pixel(ui->group->render_ctx->draw_dim.y, 1);
		ui->root->computed_rect = range_center_dim(vec2(0, 0), ui->root->computed_dim);
	}
	
	if (ui->popup_root)
	{
		ui->popup_root->computed_rect = range_center_dim(vec2(0, 0), ui->root->computed_dim);
	}
	
	ui_update_layout(ui, ui->root);
	ui_animate_elements(ui, ui->root);
	ui_handle_this_frame_input(ui, ui->root);
	ui_draw_elements(ui, ui->root);
}

#if 0
enum UI_Interaction_Flag
{
	UI_Flag_Selectable = (1 << 0),
};

#define ui_widget_interaction(ui, widget_rect, flags) _ui_widget_interaction(ui, __LINE__, widget_rect, flags)
internal Mplayer_UI_Interaction
_ui_widget_interaction(Mplayer_UI *ui, u64 id, Range2_F32 widget_rect, u32 flags)
{
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	if (!ui->disable_input)
	{
		if (ui->hot_widget_id == id && mplayer_button_released(ui->input, Key_LeftMouse))
		{
			interaction.released = 1;
		}
		
		if (is_in_range(widget_rect, ui->mouse_p))
		{
			interaction.hover = 1;
			if (ui->active_widget_id != id)
			{
				ui->active_widget_id = id;
				ui->active_t = 0;
				ui->active_dt = 8.0f;
			}
		}
		else
		{
			if (ui->active_widget_id == id)
			{
				ui->active_widget_id = 0;
			}
			
			if (has_flag(flags, UI_Flag_Selectable))
			{
				if (ui->hot_widget_id != id && 
					ui->selected_widget_id == id &&
					mplayer_button_released(ui->input, Key_LeftMouse))
				{
					ui->selected_widget_id = 0;
				}
			}
			
			if (interaction.released)
			{
				ui->hot_widget_id = 0;
			}
		}
		
		if (ui->active_widget_id == id)
		{
			ui->active_t += ui->input->frame_dt * ui->active_dt;
			ui->active_t = CLAMP(0, ui->active_t, 1.0f);
			
			if (mplayer_button_clicked(ui->input, Key_LeftMouse))
			{
				if (has_flag(flags, UI_Flag_Selectable))
				{
					ui->selected_widget_id = id;
				}
				
				ui->recent_click_time = ui->input->time;
				ui->hot_widget_id = id;
				ui->hot_t = 0;
				ui->hot_dt = 20.0f;
			}
			
			if (ui->hot_widget_id == id && mplayer_button_released(ui->input, Key_LeftMouse))
			{
				interaction.clicked = 1;
				ui->hot_widget_id = 0;
			}
		}
		
		
		if (ui->hot_widget_id == id)
		{
			interaction.pressed = true;
			ui->hot_t += ui->input->frame_dt * ui->hot_dt;
			ui->hot_t = CLAMP(0, ui->hot_t, 1.0f);
		}
	}
	return interaction;
}

#define ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, progress) _ui_slider_f32(ui, value, min, max, slider_pos, slider_dim, progress,  __LINE__)
internal Mplayer_UI_Interaction
_ui_slider_f32(Mplayer_UI *ui, f32 *value, f32 min, f32 max, V2_F32 slider_pos, V2_F32 slider_dim, b32 progress, u64 id)
{
	Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, id, range_center_dim(slider_pos, slider_dim), 0);
	V4_F32 color = vec4(0.3f, 0.3f, 0.3f, 1);
	
	V2_F32 track_dim = slider_dim;
	track_dim.width  *= 0.99f;
	track_dim.height *= 0.25f;
	
	V4_F32 track_color = vec4(0.6f, 0.6f, 0.6f, 1);
	
	V2_F32 handle_dim = vec2(0.75f * MIN(slider_dim.width, slider_dim.height));
	V2_F32 handle_pos = slider_pos;
	handle_pos.x -= 0.5f * (slider_dim.width - handle_dim.width);
	V4_F32 handle_color = vec4(0.5f, 0.5f, 0.5f, 1);
	
	V2_F32 track_final_dim = track_dim;
	V2_F32 handle_final_dim = handle_dim;
	
	if (interaction.hover)
	{
		track_final_dim.height = lerp(track_dim.height, 
			ui->active_t,
			1.25f * track_dim.height);
		
		handle_final_dim.height = lerp(handle_dim.height, 
			ui->active_t,
			1.15f * handle_dim.height);
	}
	
	if (interaction.pressed)
	{
		handle_final_dim.height = lerp(handle_final_dim.height, 
			ui->active_t,
			0.95f * handle_dim.height);
		
		handle_color = lerp(handle_color, ui->hot_t, vec4(0.7f, 0.7f, 0.7f, 1));
		
		if (slider_dim.width)
		{
			f32 click_percent = ((ui->mouse_p.x - slider_pos.x) + 0.5f * slider_dim.width) / slider_dim.width;
			click_percent = CLAMP(0.0f, click_percent, 1.0f);
			*value = lerp(min, click_percent, max);
		}
	}
	
	// NOTE(fakhri): draw slider background
	push_rect(ui->group, slider_pos, slider_dim, color, 5);
	
	// NOTE(fakhri): draw track
	push_rect(ui->group, slider_pos, track_final_dim, track_color, 5);
	handle_pos.x += map_into_range_zo(min, *value, max) * (slider_dim.width - handle_final_dim.width);
	
	if (progress)
	{
		V4_F32 progress_color = vec4(1, 1, 1, 1);
		// NOTE(fakhri): draw progress
		Range2_F32 track_rect = range_center_dim(slider_pos, track_final_dim);
		Range2_F32 progress_rect = range_cut_left(track_rect, 0.5f * track_final_dim.width + handle_pos.x - slider_pos.x).left;
		push_rect(ui->group, range_center(progress_rect), range_dim(progress_rect), progress_color, 5);
	}
	
	
	// NOTE(fakhri): draw handle
	push_rect(ui->group, handle_pos, handle_final_dim, handle_color, 0.5f * handle_final_dim.width);
	
	return interaction;
}

#define ui_input_field(ui, font, pos, field_dim, buffer, buffer_capacity) _ui_input_field(ui, font, pos, field_dim, buffer, buffer_capacity, __LINE__)
internal Mplayer_UI_Interaction
_ui_input_field(Mplayer_UI *ui, Mplayer_Font *font, V2_F32 pos, V2_F32 field_dim, String8 *buffer, u32 buffer_capacity, u32 id)
{
	Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, id, range_center_dim(pos, field_dim), UI_Flag_Selectable);
	
	V2_F32 padding    = vec2(10, 10);
	V2_F32 ui_input_dim = field_dim + padding;
	V4_F32 ui_input_color = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	
	V2_F32 final_ui_input_dim = ui_input_dim;
	
	if (interaction.hover)
	{
		final_ui_input_dim.height = lerp(final_ui_input_dim.height, 
			ui->active_t,
			1.05f * ui_input_dim.height);
	}
	
	if (interaction.pressed)
	{
		final_ui_input_dim.height = lerp(final_ui_input_dim.height, 
			ui->hot_t,
			0.99f * ui_input_dim.height);
	}
	
	V2_F32 text_dim = font_compute_text_dim(font, *buffer);
	if (ui->selected_widget_id == id)
	{
		ui_input_color = vec4(0.4f, 0.4f, 0.4f, 0.9f);
		
		if (interaction.pressed)
		{
			f32 test_x = pos.x - 0.5f * text_dim.width;
			ui->input_cursor = 0;
			for (u32 i = 0; i < buffer->len; i += 1)
			{
				f32 glyph_width = font_get_glyph_from_char(font, buffer->str[i]).advance;
				if (test_x + 0.5f * glyph_width < ui->mouse_p.x)
				{
					ui->input_cursor = i + 1;
				}
				test_x += glyph_width;
			}
		}
		
		for (Mplayer_Input_Event *event = ui->input->first_event; event; event = event->next)
		{
			if (event->consumed) continue;
			switch(event->kind)
			{
				// TODO(fakhri): text selection
				// TODO(fakhri): handle standard keyboard shortcuts (ctrl+arrows, ctrl+backspace...)
				case Event_Kind_Text:
				{
					interaction.edited = true;
					event->consumed = true;
					if (buffer->len < buffer_capacity)
					{
						if (ui->input_cursor < buffer->len)
						{
							memory_move(buffer->str + ui->input_cursor + 1,
								buffer->str + ui->input_cursor,
								buffer->len - ui->input_cursor);
						}
						
						buffer->str[ui->input_cursor] = (u8)event->text_character;
						buffer->len += 1;
						ui->input_cursor += 1;
					}
				};
				case Event_Kind_Press:
				{
					if (event->key == Key_Backspace)
					{
						event->consumed = true;
						if (ui->input_cursor && buffer->len)
						{
							interaction.edited = true;
							memory_move(buffer->str + ui->input_cursor - 1,
								buffer->str + ui->input_cursor,
								buffer->len - ui->input_cursor);
							buffer->len     -= 1;
							ui->input_cursor -= 1;
						}
					}
					else if (event->key == Key_Left)
					{
						event->consumed = true;
						if (ui->input_cursor)
							ui->input_cursor -= 1;
					}
					else if (event->key == Key_Right)
					{
						event->consumed = true;
						if (ui->input_cursor < buffer->len)
							ui->input_cursor += 1;
					}
				} break;
				
				case Event_Kind_Release: break;
				case Event_Kind_Mouse_Wheel: break;
				case Event_Kind_Mouse_Move: break;
				case Event_Kind_Null: break;
				invalid_default_case;
			}
		}
	}
	
	push_rect(ui->group, pos, final_ui_input_dim, ui_input_color);
	draw_outline(ui->group, pos, final_ui_input_dim, 
		vec4(0, 0, 0, 1), 1);
	
	draw_text(ui->group, font, pos, vec4(1, 1, 1, 1), fancy_str8(*buffer), Text_Render_Flag_Centered);
	if (ui->selected_widget_id == id)
	{
		f32 blink_t = (sin_f(2 * PI32 * (ui->input->time - ui->recent_click_time)) + 1) / 2;
		if (ui->input->time - ui->recent_click_time > 5)
		{
			blink_t = 1.0f;
		}
		V4_F32 cursor_color = lerp(ui_input_color,
			blink_t,
			vec4(0, 0, 0, 1));
		if (ui->input_cursor > buffer->len)
		{
			ui->input_cursor = (u32)buffer->len;
		}
		
		V2_F32 cursor_pos = vec2(pos.x - 0.5f * text_dim.width + font_compute_text_dim(font, str8(buffer->str, MIN(buffer->len, ui->input_cursor))).width,
			pos.y);
		push_rect(ui->group, cursor_pos, vec2(2.1f, 1.25f * text_dim.height), cursor_color);
	}
	return interaction;
}

#define ui_button(ui, font, text, pos) _ui_button(ui, font, text, pos, __LINE__)
internal Mplayer_UI_Interaction
_ui_button(Mplayer_UI *ui, Mplayer_Font *font,  String8 text, V2_F32 pos, u64 id)
{
	V2_F32 padding    = vec2(10, 10);
	V2_F32 button_dim = font_compute_text_dim(font, text) + padding;
	
	Mplayer_UI_Interaction interaction = _ui_widget_interaction(ui, id, range_center_dim(pos, button_dim), 0);
	
	V2_F32 final_button_dim = button_dim;
	
	if (interaction.hover)
	{
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->active_t,
			1.05f * button_dim.height);
	}
	
	if (interaction.pressed)
	{
		final_button_dim.height = lerp(final_button_dim.height, 
			ui->hot_t,
			0.95f * button_dim.height);
	}
	
	V4_F32 button_bg_color = vec4(0.3f, 0.3f, 0.3f, 1);
	V4_F32 text_color = vec4(1.0f, 1.0f, 1.0f, 1);
	push_rect(ui->group, pos, final_button_dim, button_bg_color);
	draw_text(ui->group, font, pos, text_color, fancy_str8(text), Text_Render_Flag_Centered);
	return interaction;
}


internal Mplayer_UI_Interaction 
ui_list_option_begin(Mplayer_UI *ui, u32 id)
{
	V2_F32 option_dim = ui->option_dim;
	V2_F32 padding = ui->option_padding;
	Range2_F32 available_space = ui->option_available_space;
	
	Range2_F32 item_rect = range_center_dim(ui->option_pos, option_dim);
	Range2_F32 visible_range = range_intersection(item_rect, available_space);
	
	Mplayer_UI_Interaction interaction = ZERO_STRUCT;
	if (is_range_intersect(item_rect, available_space))
	{
		interaction = _ui_widget_interaction(ui, id, visible_range, 0);
	}
	else
	{
		interaction.not_visible = true;
	}
	
	return interaction;
}

internal void
ui_list_option_advance(Mplayer_UI *ui)
{
	V2_F32 option_dim = ui->option_dim;
	V2_F32 padding = ui->option_padding;
	Range2_F32 available_space = ui->option_available_space;
	
	ui->option_pos.x += option_dim.width + padding.width;
	if (ui->option_pos.x + 0.5f * option_dim.width > available_space.max_x)
	{
		ui->option_pos.x = ui->option_start_pos.x;
		ui->option_pos.y -= option_dim.height + padding.height;
	}
}

internal u32
ui_begin_list(Mplayer_UI *ui, Mplayer_Scroll_Data *scroll, V2_F32 option_dim, Range2_F32 available_space, u32 options_count, V2_F32 padding = vec2(0, 0))
{
	for (Mplayer_Input_Event *event = ui->input->first_event; event; event = event->next)
	{
		if (event->consumed) continue;
		if (event->kind == Event_Kind_Mouse_Wheel)
		{
			event->consumed = true;
			
			scroll->target -= event->scroll.y;
		}
	}
	mplayer_animate_scroll(scroll, ui->input->frame_dt);
	
	V2_F32 available_space_dim = range_dim(available_space);
	V2_F32 option_start_pos = range_center(available_space);
	option_start_pos.x -= 0.5f * (available_space_dim.width - option_dim.width) - padding.width;
	option_start_pos.y += 0.5f * (available_space_dim.height - option_dim.height) - padding.height;
	
	u32 options_per_row = u32(available_space_dim.width / (option_dim.width + padding.width));
	options_per_row = MAX(options_per_row, 1);
	
	u32 rows_count = options_count / options_per_row + !!(options_count % options_per_row);
	
	scroll->target = CLAMP(0, scroll->target, rows_count - 1);
	
	ui->option_pos = option_start_pos;
	ui->option_pos.y += scroll->t * (option_dim.height + padding.height);
	
	ui->option_start_pos = option_start_pos;
	ui->option_dim = option_dim;
	ui->option_padding = padding;
	ui->option_available_space = available_space;
	
	u32 index = 0;
	for (; index < options_count; index += 1)
	{
		Range2_F32 item_rect = range_center_dim(ui->option_pos, option_dim);
		Range2_F32 visible_range = range_intersection(item_rect, available_space);
		if (is_range_intersect(item_rect, available_space))
		{
			break;
		}
		ui_list_option_advance(ui);
	}
	
	return index;
}

struct Mplayer_List_Option_Result
{
	b32 handled;
	b32 selected;
};

internal Mplayer_List_Option_Result
mplayer_album_list_option(Render_Group *group, Mplayer_Context *mplayer, Mplayer_Item_ID album_id)
{
	Mplayer_List_Option_Result result = ZERO_STRUCT;
	Mplayer_Album *album = mplayer_album_by_id(&mplayer->library, album_id);
	
	Mplayer_UI_Interaction interaction = ui_list_option_begin(&mplayer->ui, (u32)int_from_ptr(album));
	if (!interaction.not_visible)
	{
		result.handled = true;
		
		V4_F32 bg_color = vec4(1, 1, 1, 0.35f);
		if (interaction.hover)
		{
			bg_color.a = 0.80f;
		}
		
		if (interaction.pressed)
		{
			bg_color.a = 0;
			result.selected = true;
		}
		
		mpalyer_draw_item_image(group, mplayer, &album->header, mplayer->ui.option_pos, mplayer->ui.option_dim, bg_color, bg_color);
		
		V2_F32 name_dim = font_compute_text_dim(&mplayer->font, album->name);
		
		Range2_F32 item_rect = range_center_dim(mplayer->ui.option_pos, mplayer->ui.option_dim);
		Range2_F32_Cut _cut = range_cut_top(item_rect, 10); // padding
		_cut = range_cut_top(_cut.bottom, 30);
		
		Range2_F32 album_name_rect = _cut.top;
		Range2_F32 album_image_rect = _cut.bottom;
		draw_text(group, &mplayer->font, range_center(album_name_rect), vec4(1, 1, 1, 1), fancy_str8(album->name), Text_Render_Flag_Centered);
		
		ui_list_option_advance(&mplayer->ui);
	}
	
	return result;
}

internal Mplayer_List_Option_Result
mplayer_artist_list_option(Render_Group *group, Mplayer_Context *mplayer, Mplayer_Item_ID artist_id)
{
	Mplayer_List_Option_Result result = ZERO_STRUCT;
	Mplayer_Artist *artist = mplayer_artist_by_id(&mplayer->library, artist_id);
	
	Mplayer_UI_Interaction interaction = ui_list_option_begin(&mplayer->ui, (u32)int_from_ptr(artist));
	if (!interaction.not_visible)
	{
		result.handled = true;
		
		V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
		if (interaction.hover)
		{
			bg_color = vec4(0.2f, 0.2f,0.2f, 1);
		}
		
		if (interaction.pressed)
		{
			bg_color = vec4(0.14f, 0.14f,0.14f, 1);
			result.selected = true;
		}
		
		push_rect(group, mplayer->ui.option_pos, mplayer->ui.option_dim, bg_color);
		V2_F32 name_dim = font_compute_text_dim(&mplayer->font, artist->name);
		
		Range2_F32 item_rect = range_center_dim(mplayer->ui.option_pos, mplayer->ui.option_dim);
		Range2_F32_Cut _cut = range_cut_top(item_rect, 10); // padding
		_cut = range_cut_top(_cut.bottom, 30);
		
		Range2_F32 artist_name_rect = _cut.top;
		Range2_F32 artist_image_rect = _cut.bottom;
		draw_text(group, &mplayer->font, range_center(artist_name_rect), vec4(1, 1, 1, 1), fancy_str8(artist->name), Text_Render_Flag_Centered);
		
		ui_list_option_advance(&mplayer->ui);
	}
	
	return result;
}

internal Mplayer_List_Option_Result
mplayer_track_list_option(Render_Group *group, Mplayer_Context *mplayer, Mplayer_Item_ID track_id)
{
	Mplayer_List_Option_Result result = ZERO_STRUCT;
	Mplayer_Track *track = mplayer_track_by_id(&mplayer->library, track_id);
	Mplayer_UI_Interaction interaction = ui_list_option_begin(&mplayer->ui, (u32)int_from_ptr(track));
	if (!interaction.not_visible)
	{
		result.handled = true;
		
		V4_F32 bg_color = vec4(0.15f, 0.15f,0.15f, 1);
		if (track_id == mplayer->current_music_id)
		{
			bg_color = vec4(0.f, 0.f,0.f, 1);
		}
		
		if (interaction.hover)
		{
			bg_color = vec4(0.2f, 0.2f,0.2f, 1);
		}
		
		if (interaction.pressed)
		{
			bg_color = vec4(0.14f, 0.14f,0.14f, 1);
			result.selected = true;
		}
		
		// NOTE(fakhri): background
		push_rect(group, mplayer->ui.option_pos, mplayer->ui.option_dim, bg_color);
		
		Range2_F32 item_rect = range_center_dim(mplayer->ui.option_pos, mplayer->ui.option_dim);
		Range2_F32_Cut _cut = range_cut_left(item_rect, 20); // horizontal padding
		
		_cut = range_cut_left(_cut.right, 500);
		Range2_F32 name_rect = _cut.left;
		draw_text(group, &mplayer->font, vec2(name_rect.min_x, range_center(name_rect).y), vec4(1, 1, 1, 1), fancy_str8(track->title, range_dim(name_rect).width), Text_Render_Flag_Limit_Width);
		
		draw_outline(group, mplayer->ui.option_pos, mplayer->ui.option_dim, vec4(0, 0, 0, 1), 1);
		
		ui_list_option_advance(&mplayer->ui);
	}
	
	return result;
}
#endif
