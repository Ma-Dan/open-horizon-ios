@sampler morph "morph"
@uniform morphs "morphs"
@uniform morphs_count "morphs count"=0,1

/*
@all
uniform sampler2D morph;
@fragment
vec4 workaround=texture2D(morph,vec2(0.0,0.0));
*/

@vertex
uniform sampler2D morph;
uniform vec4 morphs_count;
uniform vec4 morphs[16];

//ToDo

vec3 apply_morphs(vec3 p,float vidx)
{
    for(int i=0;i<int(morphs_count.x);++i)
    {
        vec2 mtc;
        mtc.x=vidx+morphs[i].y;
        mtc.y=floor(mtc.x/morphs_count.y);
        mtc.x-=mtc.y*morphs_count.y;

        vec3 mv = texture2D(morph,(mtc+vec2(0.5))/morphs_count.yz).xyz;
        p += mv * morphs[i].x;
    }

    return p;
}
