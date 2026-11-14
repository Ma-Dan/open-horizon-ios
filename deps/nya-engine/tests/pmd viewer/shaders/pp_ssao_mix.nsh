@all

varying vec2 tc;

@vertex

void main()
{
    tc=gl_MultiTexCoord0.xy;
    gl_Position=gl_Vertex;
}

@sampler ssao_map "ssao"
@sampler base_map "base"

@fragment

uniform sampler2D ssao_map;
uniform sampler2D base_map;

void main()
{
    float ssao = texture2D(ssao_map,tc).r;
    vec4 color = texture2D(base_map,tc);

    float value = 0.3;
	color.rgb = mix(1.0, ssao, value) * color.rgb;
    gl_FragColor = color;
}
