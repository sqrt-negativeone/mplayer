
#ifdef VERTEX_SHADER

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;


layout (location = 3) in vec2 a_rect_center;
layout (location = 4) in vec2 a_rect_dim;
layout (location = 5) in float a_roundness;

out vec4 color;
out vec2 frag_pos;
out vec2 tex_uv;
uniform mat4 clip;

void main()
{
	vec4 world_pos = vec4(a_pos, 1);
  gl_Position = clip * world_pos;
	
  tex_uv = a_uv;
	frag_pos = a_pos.xy;
	
	color = a_color;
}

#endif

#ifdef FRAGMENT_SHADER

in vec2 frag_pos;
in vec4 color;
in vec2 tex_uv;

out vec4 frag_color;

uniform sampler2D tex0;
uniform vec2  rect_cent;
uniform vec2  rect_dim;
uniform float roundness = 0; // for round corners

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
	
	
	vec4 sampled_tex_color = texture(tex0, tex_uv);
	if (sampled_tex_color.a > 0)
	{
		frag_color = color * sampled_tex_color;
		
	}
	else
	{
		discard;
	}
}

#endif
