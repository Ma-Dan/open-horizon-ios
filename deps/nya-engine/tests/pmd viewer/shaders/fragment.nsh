
@fragment

void main()
{
    vec4 c = texture2D(base,tc);
    c.a *= diff_k.a;
    if(c.a < 0.01)
        discard;

    float s = 0.0;
    float l = 0.5 + 0.5 * dot(light_dir.xyz, normal);
    vec3 t = texture2D(toon,vec2(s, l)).rgb;

    vec4 e = texture2D(env, env_tc);
    c=mix(c,c*e,env_p.x);
    c.rgb+=env_p.y*e.rgb;

    vec3 eye = normalize(cam_pos.xyz - pos);
    float ndh = dot(normal, normalize(light_dir.xyz + eye));
    vec3 spec = spec_k.rgb * max(pow(ndh, spec_k.a), 0.0);

    c.rgb *= clamp(amb_k.rgb + (diff_k.rgb) * light_color.rgb, vec3(0.0), vec3(1.0));
    c.rgb += spec * light_color.rgb;
    c.rgb *= t;

    gl_FragColor = c;
}
