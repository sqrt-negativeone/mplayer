
internal b32
mplayer_button_clicked(Mplayer_Input_Key button)
{
	b32 result = !button.was_down && button.is_down;
	return result;
}

internal b32
mplayer_button_released(Mplayer_Input_Key button)
{
	b32 result = !button.is_down && button.was_down;
	return result;
}

internal b32
mplayer_keyboard_button_released(Mplayer_Input *input, Mplayer_Keyboard_Key_Kind key)
{
	b32 result = mplayer_button_released(input->keyboard_buttons[key]);
	return result;
}

internal b32
mplayer_keyboard_button_clicked(Mplayer_Input *input, Mplayer_Keyboard_Key_Kind key)
{
	b32 result = mplayer_button_clicked(input->keyboard_buttons[key]);
	return result;
}

internal b32
mplayer_mouse_button_released(Mplayer_Input *input, Mplayer_Mouse_Key_Kind key)
{
	b32 result = mplayer_button_released(input->mouse_buttons[key]);
	return result;
}

internal b32
mplayer_mouse_button_clicked(Mplayer_Input *input, Mplayer_Mouse_Key_Kind key)
{
	b32 result = mplayer_button_clicked(input->mouse_buttons[key]);
	return result;
}


internal void
push_input_event(Mplayer_Input *input, Mplayer_Input_Event *event)
{
	if (!input->last_event)
	{
		input->first_event = input->last_event = event;
	}
	else
	{
		input->last_event->next = event;
		input->last_event       = event;
	}
}
