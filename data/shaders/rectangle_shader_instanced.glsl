
#ifdef VERTEX_SHADER

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;


layout (location = 3) in vec2 a_rect_center;
layout (location = 4) in vec2 a_rect_dim;
layout (location = 5) in float a_roundness;
layout (location = 6) in int  a_textured;

uniform mat4 clip;

out vec4 color;
out vec2 frag_pos;
out vec2 tex_uv;
out vec2  rect_cent;
out vec2  rect_dim;
out float roundness; // for round corners
out float is_textured; // for round corners

void main()
{
	vec4 world_pos = vec4(a_pos, 1);
  gl_Position = clip * world_pos;
	
  tex_uv = a_uv;
	frag_pos = a_pos.xy;
	
	color     = a_color;
	rect_cent = a_rect_center;
	rect_dim  = a_rect_dim;
	roundness = a_roundness;
	is_textured  = a_textured;
}

#endif

#ifdef FRAGMENT_SHADER

in vec2 frag_pos;
in vec4 color;
in vec2 tex_uv;

out vec4 frag_color;

uniform sampler2D tex0;
in vec2  rect_cent;
in vec2  rect_dim;
in float roundness; // for round corners
in float is_textured; // for round corners

void main()
{
	// NOTE(fakhri): rounded corners
	{
		vec2 half_size = rect_dim * 0.5;
    vec2 local_pos = abs(frag_pos - rect_cent) - half_size + vec2(roundness);
		
    // Check if the fragment is inside the rounded rectangle area
    float dist = length(max(local_pos, vec2(0.0))) - roundness;
		if (dist > 0)
		{
			discard;
		}
	}
	
	frag_color = color;
	if (is_textured != 0)
	{
		frag_color *= texture(tex0, tex_uv);
	}
	
	if (frag_color.a == 0)
	{
		discard;
	}
}

#endif
