
enum MPlayer_Input_Key_Kind
{
	Key_Unknown,
  Key_Esc,
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,
  Key_F13,
  Key_F14,
  Key_F15,
  Key_F16,
  Key_F17,
  Key_F18,
  Key_F19,
  Key_F20,
  Key_F21,
  Key_F22,
  Key_F23,
  Key_F24,
  Key_GraveAccent,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_Minus,
  Key_Plus,
  Key_Backspace,
  Key_Delete,
  Key_Tab,
  Key_A,
  Key_B,
  Key_C,
  Key_D,
  Key_E,
  Key_F,
  Key_G,
  Key_H,
  Key_I,
  Key_J,
  Key_K,
  Key_L,
  Key_M,
  Key_N,
  Key_O,
  Key_P,
  Key_Q,
  Key_R,
  Key_S,
  Key_T,
  Key_U,
  Key_V,
  Key_W,
  Key_X,
  Key_Y,
  Key_Z,
  Key_Space,
  Key_Enter,
  Key_Ctrl,
  Key_Shift,
  Key_Alt,
  Key_Up,
  Key_Left,
  Key_Down,
  Key_Right,
  Key_PageUp,
  Key_PageDown,
  Key_Home,
  Key_End,
  Key_ForwardSlash,
  Key_Period,
  Key_Comma,
  Key_Quote,
  Key_LeftBracket,
  Key_RightBracket,
  Key_SemiColon,
	
  Key_LeftMouse,
  Key_MiddleMouse,
  Key_RightMouse,
  
	Key_COUNT,
};

struct MPlayer_Input_Button
{
	b32 is_down;
	b32 was_down;
};

enum MPlayer_Input_Event_Kind
{
	Event_Kind_Null,
	Event_Kind_Press,
	Event_Kind_Release,
	Event_Kind_Text,
	Event_Kind_Mouse_Wheel,
};

enum MPlayer_Key_Modifier_Flag
{
	Modifier_Ctrl,
	Modifier_Shift,
	Modifier_Alt,
};

struct MPlayer_Input_Event
{
	MPlayer_Input_Event *next;
	MPlayer_Input_Event_Kind kind;
	u32 modifiers;
	MPlayer_Input_Key_Kind key;
	u32 text_character;
	V2_F32 pos;
	V2_F32 scroll;
};

struct MPlayer_Input
{
	V2_F32 mouse_clip_pos;
	MPlayer_Input_Event *first_event;
	MPlayer_Input_Event *last_event;
	
	MPlayer_Input_Button buttons[Key_COUNT];
};


internal void
push_input_event(MPlayer_Input *input, MPlayer_Input_Event *event)
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