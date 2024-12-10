
enum Mplayer_Keyboard_Key_Kind
{
	Keyboard_Unknown,
  Keyboard_Esc,
  Keyboard_F1,
  Keyboard_F2,
  Keyboard_F3,
  Keyboard_F4,
  Keyboard_F5,
  Keyboard_F6,
  Keyboard_F7,
  Keyboard_F8,
  Keyboard_F9,
  Keyboard_F10,
  Keyboard_F11,
  Keyboard_F12,
  Keyboard_F13,
  Keyboard_F14,
  Keyboard_F15,
  Keyboard_F16,
  Keyboard_F17,
  Keyboard_F18,
  Keyboard_F19,
  Keyboard_F20,
  Keyboard_F21,
  Keyboard_F22,
  Keyboard_F23,
  Keyboard_F24,
  Keyboard_GraveAccent,
  Keyboard_0,
  Keyboard_1,
  Keyboard_2,
  Keyboard_3,
  Keyboard_4,
  Keyboard_5,
  Keyboard_6,
  Keyboard_7,
  Keyboard_8,
  Keyboard_9,
  Keyboard_Minus,
  Keyboard_Plus,
  Keyboard_Backspace,
  Keyboard_Delete,
  Keyboard_Tab,
  Keyboard_A,
  Keyboard_B,
  Keyboard_C,
  Keyboard_D,
  Keyboard_E,
  Keyboard_F,
  Keyboard_G,
  Keyboard_H,
  Keyboard_I,
  Keyboard_J,
  Keyboard_K,
  Keyboard_L,
  Keyboard_M,
  Keyboard_N,
  Keyboard_O,
  Keyboard_P,
  Keyboard_Q,
  Keyboard_R,
  Keyboard_S,
  Keyboard_T,
  Keyboard_U,
  Keyboard_V,
  Keyboard_W,
  Keyboard_X,
  Keyboard_Y,
  Keyboard_Z,
  Keyboard_Space,
  Keyboard_Enter,
  Keyboard_Ctrl,
  Keyboard_Shift,
  Keyboard_Alt,
  Keyboard_Up,
  Keyboard_Left,
  Keyboard_Down,
  Keyboard_Right,
  Keyboard_PageUp,
  Keyboard_PageDown,
  Keyboard_Home,
  Keyboard_End,
  Keyboard_ForwardSlash,
  Keyboard_Period,
  Keyboard_Comma,
  Keyboard_Quote,
  Keyboard_LeftBracket,
  Keyboard_RightBracket,
  Keyboard_SemiColon,
	
	Keyboard_COUNT,
};


enum Mplayer_Mouse_Key_Kind
{
  Mouse_Left,
  Mouse_Middle,
  Mouse_Right,
  Mouse_M4,
  Mouse_M5,
	
	Mouse_COUNT
};

struct Mplayer_Input_Key
{
	b32 is_down;
	b32 was_down;
};

enum Mplayer_Input_Event_Kind
{
	Event_Kind_Null,
	Event_Kind_Keyboard_Key,
	Event_Kind_Mouse_Key,
	Event_Kind_Text,
	Event_Kind_Mouse_Wheel,
	Event_Kind_Mouse_Move,
};

enum Mplayer_Key_Modifier_Flag
{
	Modifier_Ctrl  = (1 << 0),
	Modifier_Shift = (1 << 1),
	Modifier_Alt   = (1 << 2),
};

struct Mplayer_Input_Event
{
	Mplayer_Input_Event *next;
	Mplayer_Input_Event *prev;
	Mplayer_Input_Event_Kind kind;
	b32 is_down;
	u32 modifiers;
	u32 key;
	u32 text_character;
	V2_F32 pos;
	V2_F32 scroll;
};

struct Mplayer_Input
{
	f32 time;
	f32 frame_dt;
	V2_F32 mouse_clip_pos;
	Mplayer_Input_Event *first_event;
	Mplayer_Input_Event *last_event;
	
	Mplayer_Input_Key keyboard_buttons[Keyboard_COUNT];
	Mplayer_Input_Key mouse_buttons[Mouse_COUNT];
};

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
