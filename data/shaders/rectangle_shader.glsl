
#ifdef VERTEX_SHADER
layout (location = 0) in vec2 a_pt;
layout (location = 1) in vec2 a_uv;

out vec2 frag_pos;
out vec2 tex_uv;
uniform mat4 model;
uniform mat4 clip;
uniform vec2 uv_offset = vec2(0, 0);
uniform vec2 uv_scale  = vec2(1, 1);

void main()
{
	vec4 world_pos = model * vec4(a_pt, 0, 1);
  gl_Position = clip * model * vec4(a_pt, 0, 1);
  tex_uv = uv_offset + uv_scale * a_uv;
	frag_pos = world_pos.xy;
}
#endif

#ifdef FRAGMENT_SHADER

in vec2 frag_pos;

in vec2 tex_uv;
out vec4 frag_color;
uniform sampler2D tex0;
uniform vec4 color;

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
