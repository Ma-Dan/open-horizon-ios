//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "render_opengl.h"
#include "platform_specific_gl.h"
#include "memory/tmp_buffer.h"
#include "bitmap.h"
#include "fbo.h"

namespace nya_render
{

bool render_opengl::is_available() const
{
    return true; //ToDo
}

namespace
{
    render_api_interface::state applied_state;
    bool ignore_cache = true;
	bool ignore_cache_vp = true;
    vbo::layout applied_layout;
    bool was_fbo_without_color=false;

    bool check_init_fbo();

    int gl_cube_type(int side)
    {
        switch(side)
        {
            case fbo::cube_positive_x: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            case fbo::cube_negative_x: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
            case fbo::cube_positive_y: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
            case fbo::cube_negative_y: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
            case fbo::cube_positive_z: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
            case fbo::cube_negative_z: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        }

        return GL_TEXTURE_2D;
    }

#ifndef NO_EXTENSIONS_INIT
    PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate=NULL;
#endif

//shaders

#ifndef NO_EXTENSIONS_INIT
    PFNGLGENPROGRAMSARBPROC glGenProgramsARB = NULL;
    PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
    PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
    PFNGLDELETEOBJECTARBPROC glDeleteObjectARB = NULL;
    PFNGLDETACHOBJECTARBPROC glDetachObjectARB = NULL;
    PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = NULL;
    PFNGLSHADERSOURCEARBPROC glShaderSourceARB = NULL;
    PFNGLCOMPILESHADERARBPROC glCompileShaderARB = NULL;
    PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = NULL;
    PFNGLATTACHOBJECTARBPROC glAttachObjectARB = NULL;
    PFNGLLINKPROGRAMARBPROC glLinkProgramARB = NULL;
    PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = NULL;
    PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB = NULL;
    PFNGLUNIFORM1IARBPROC glUniform1iARB = NULL;
    PFNGLUNIFORM1FVARBPROC glUniform1fvARB = NULL;
    PFNGLUNIFORM2FVARBPROC glUniform2fvARB = NULL;
    PFNGLUNIFORM3FVARBPROC glUniform3fvARB = NULL;
    PFNGLUNIFORM4FVARBPROC glUniform4fvARB = NULL;
    PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB = NULL;
    PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = NULL;
    PFNGLGETINFOLOGARBPROC glGetInfoLogARB = NULL;
    PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = NULL;
  #ifdef OPENGL3
    PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB = NULL;
  #endif
#endif

#if defined OPENGL_ES || (defined __APPLE__ && defined OPENGL3)
  #define GLhandleARB GLuint

  #define glCreateProgramObjectARB glCreateProgram
  #define glUseProgramObjectARB glUseProgram
  #define glLinkProgramARB glLinkProgram
  #define glValidateProgramARB glValidateProgram

  #define glCreateShaderObjectARB glCreateShader
  #define glDeleteObjectARB glDeleteShader
  #define glAttachObjectARB glAttachShader
  #define glDetachObjectARB glDetachShader
  #define glShaderSourceARB glShaderSource
  #define glCompileShaderARB glCompileShader
  #define glGetInfoLogARB glGetShaderInfoLog

  #define glBindAttribLocationARB glBindAttribLocation

  #define glGetUniformLocationARB glGetUniformLocation
  #define glUniform1iARB glUniform1i
  #define glUniform1fvARB glUniform1fv
  #define glUniform2fvARB glUniform2fv
  #define glUniform3fvARB glUniform3fv
  #define glUniform4fvARB glUniform4fv
  #define glUniformMatrix4fvARB glUniformMatrix4fv

  #ifndef GL_VERTEX_SHADER_ARB
    #define GL_VERTEX_SHADER_ARB GL_VERTEX_SHADER
  #endif

  #ifndef GL_FRAGMENT_SHADER_ARB
    #define GL_FRAGMENT_SHADER_ARB GL_FRAGMENT_SHADER
  #endif

  #ifndef GL_ARB_shader_objects
    #define GL_OBJECT_COMPILE_STATUS_ARB GL_COMPILE_STATUS
    #define GL_OBJECT_INFO_LOG_LENGTH_ARB GL_INFO_LOG_LENGTH
    #define GL_OBJECT_LINK_STATUS_ARB GL_LINK_STATUS
    #define GL_OBJECT_VALIDATE_STATUS_ARB GL_VALIDATE_STATUS
  #endif

    #define glGetObjectParam glGetProgramiv
    #define glGetShaderParam glGetShaderiv
#else
    #define glGetObjectParam glGetObjectParameterivARB
    #define glGetShaderParam glGetObjectParameterivARB
#endif

    bool check_init_shaders()
    {
        static bool initialised=false;
        static bool failed=true;
        if(initialised)
            return !failed;

    #ifndef NO_EXTENSIONS_INIT
        if(!(glGenProgramsARB          =(PFNGLGENPROGRAMSARBPROC)          get_extension("glGenProgramsARB"))) return false;
        if(!(glDeleteProgramsARB       =(PFNGLDELETEPROGRAMSARBPROC)       get_extension("glDeleteProgramsARB"))) return false;
        if(!(glBindProgramARB          =(PFNGLBINDPROGRAMARBPROC)          get_extension("glBindProgramARB"))) return false;
        if(!(glDeleteObjectARB         =(PFNGLDELETEOBJECTARBPROC)         get_extension("glDeleteObjectARB"))) return false;
        if(!(glDetachObjectARB         =(PFNGLDETACHOBJECTARBPROC)         get_extension("glDetachObjectARB"))) return false;
        if(!(glCreateShaderObjectARB   =(PFNGLCREATESHADEROBJECTARBPROC)   get_extension("glCreateShaderObjectARB"))) return false;
        if(!(glShaderSourceARB         =(PFNGLSHADERSOURCEARBPROC)         get_extension("glShaderSourceARB"))) return false;
        if(!(glCompileShaderARB        =(PFNGLCOMPILESHADERARBPROC)        get_extension("glCompileShaderARB"))) return false;
        if(!(glCreateProgramObjectARB  =(PFNGLCREATEPROGRAMOBJECTARBPROC)  get_extension("glCreateProgramObjectARB"))) return false;
        if(!(glAttachObjectARB         =(PFNGLATTACHOBJECTARBPROC)         get_extension("glAttachObjectARB"))) return false;
        if(!(glLinkProgramARB          =(PFNGLLINKPROGRAMARBPROC)          get_extension("glLinkProgramARB"))) return false;
        if(!(glUseProgramObjectARB     =(PFNGLUSEPROGRAMOBJECTARBPROC)     get_extension("glUseProgramObjectARB" ))) return false;
        if(!(glValidateProgramARB      =(PFNGLVALIDATEPROGRAMARBPROC)      get_extension("glValidateProgramARB" ))) return false;
        if(!(glUniform1iARB            =(PFNGLUNIFORM1IARBPROC)            get_extension("glUniform1iARB"))) return false;
        if(!(glUniform1fvARB           =(PFNGLUNIFORM1FVARBPROC)           get_extension("glUniform1fvARB"))) return false;
        if(!(glUniform2fvARB           =(PFNGLUNIFORM2FVARBPROC)           get_extension("glUniform2fvARB"))) return false;
        if(!(glUniform3fvARB           =(PFNGLUNIFORM3FVARBPROC)           get_extension("glUniform3fvARB"))) return false;
        if(!(glUniform4fvARB           =(PFNGLUNIFORM4FVARBPROC)           get_extension("glUniform4fvARB"))) return false;
        if(!(glUniformMatrix4fvARB     =(PFNGLUNIFORMMATRIX4FVARBPROC)     get_extension("glUniformMatrix4fvARB"))) return false;
        if(!(glGetObjectParameterivARB =(PFNGLGETOBJECTPARAMETERIVARBPROC) get_extension("glGetObjectParameterivARB"))) return false;
        if(!(glGetInfoLogARB           =(PFNGLGETINFOLOGARBPROC)           get_extension("glGetInfoLogARB"))) return false;
        if(!(glGetUniformLocationARB   =(PFNGLGETUNIFORMLOCATIONARBPROC)   get_extension("glGetUniformLocationARB"))) return false;
      #ifdef OPENGL3
        if(!(glBindAttribLocationARB =(PFNGLBINDATTRIBLOCATIONARBPROC)   get_extension("glBindAttribLocationARB"))) return false;
      #endif
    #endif
        failed=false;
        initialised=true;

#ifdef OPENGL_ES
        //TEMP DEBUG (iOS): ES2 link limits for diagnosing empty-info-log failures
        if(getenv("OH_TRACE_DRAW"))
        {
            GLint v=0;
            glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,&v);
            fprintf(stderr,"OH_GLLIMIT max_vertex_uniform_vectors=%d\n",v);
            glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,&v);
            fprintf(stderr,"OH_GLLIMIT max_fragment_uniform_vectors=%d\n",v);
            glGetIntegerv(GL_MAX_VARYING_VECTORS,&v);
            fprintf(stderr,"OH_GLLIMIT max_varying_vectors=%d\n",v);
            glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&v);
            fprintf(stderr,"OH_GLLIMIT max_vertex_attribs=%d\n",v);
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&v);
            fprintf(stderr,"OH_GLLIMIT max_texture_image_units=%d\n",v);
            glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,&v);
            fprintf(stderr,"OH_GLLIMIT max_vertex_texture_image_units=%d\n",v);
        }
#endif
        return true;
    }

    void set_shader(int idx);

    struct shader_obj
    {
    public:
        shader_obj(): program(0)
        {
            memset(objects,0,sizeof(objects));
    #ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
            mat_mvp=mat_mv=mat_p= -1;
    #endif
        }

        GLhandleARB program,objects[shader::program_types_count];

        struct uniform: public shader::uniform { int handler,cache_idx; };
        std::vector<uniform> uniforms;
        std::vector<float> uniform_cache;

    #ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
        int mat_mvp,mat_mv,mat_p;
    #endif

        void release()
        {
            for(int i=0;i<shader::program_types_count;++i)
            {
                if(!objects[i])
                    continue;

                glDetachObjectARB(program,objects[i]);
                glDeleteObjectARB(objects[i]);
            }

            if( program )
                glDeleteObjectARB(program);

            *this=shader_obj();
        }
    };
    render_objects<shader_obj> shaders;

    void set_shader(int idx)
    {
        if(idx==applied_state.shader)
            return;

        if(!check_init_shaders())
            return;

        if(idx<0)
        {
            glUseProgramObjectARB(0);
            applied_state.shader= -1;
            return;
        }

        shader_obj &shdr=shaders.get(idx);
        glUseProgramObjectARB(shdr.program);
        if(!shdr.program)
            applied_state.shader= -1;
        else
            applied_state.shader=idx;
    }

    int active_layer=-1;

    void gl_select_multitex_layer(int idx)
    {
        if(idx==active_layer)
            return;

        active_layer=idx;

    #if defined OPENGL_ES || defined NO_EXTENSIONS_INIT
        glActiveTexture(GL_TEXTURE0+idx);
    #else
        static PFNGLACTIVETEXTUREARBPROC tex_glActiveTexture=0;

        static bool initialised=false;
        if(tex_glActiveTexture!=0)
        {
            tex_glActiveTexture(GL_TEXTURE0_ARB+idx);
            return;
        }

        if(initialised)
            return;

        tex_glActiveTexture=(PFNGLACTIVETEXTUREARBPROC)get_extension("glActiveTextureARB");
        initialised=true;
        gl_select_multitex_layer(idx);
    #endif
    }
}

int render_opengl::create_shader(const char *vertex,const char *fragment)
{
    if(!check_init_shaders())
        return -1;

    const int idx=shaders.add();
    shader_obj &shdr=shaders.get(idx);

    shdr.program=glCreateProgramObjectARB();
    if(!shdr.program)
    {
        log()<<"Unable to create shader program object\n";
        shaders.remove(idx);
        return -1;
    }

    const static char type_str[][12]={"vertex","pixel","geometry","tesselation"};

    std::string last_converted_code[2]; //TEMP DEBUG (iOS): for link-failure dumps

    for(int i=0;i<2;++i)
    {
        shader_code_parser parser(i==0?vertex:fragment);
        shader::program_type type=i==0?shader::vertex:shader::pixel;

#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
  #ifdef OPENGL_ES
    #ifndef __APPLE__
        parser.fix_per_component_functions(); //some droids despise glsl specs
    #endif

        //ES2 has no gl_FragDepth; the parser rewrites the reference either to
        //gl_FragDepthEXT (extension present) or to a dummy float (pass loses
        //the depth write but still renders)
        if(i==1) //pixel shader only
            parser.replace_frag_depth(has_extension("GL_EXT_frag_depth"));

        if(!parser.convert_to_glsl_es2())
        {
            log()<<"Unable to add shader program: cannot convert shader code to glsl for es2\n";
            log()<<parser.get_error()<<"\n";
            shaders.remove(idx);
            return -1;
        }

  #else
        if(!parser.convert_to_glsl3())
        {
            log()<<"Unable to add shader program: cannot convert shader code to glsl3\n";
            log()<<parser.get_error()<<"\n";
            shaders.remove(idx);
            return -1;
        }
  #endif
#else
        parser.convert_to_glsl();
#endif
        const char *code=parser.get_code();
        last_converted_code[i==0?0:1]=code; //TEMP DEBUG (iOS): for link-failure dumps

        GLenum gl_type=type==shader::pixel?GL_FRAGMENT_SHADER_ARB:GL_VERTEX_SHADER_ARB; //ToDo: switch all cases

        GLhandleARB object = glCreateShaderObjectARB(gl_type);
        glShaderSourceARB(object,1,&code,0);
        glCompileShaderARB(object);
        GLint compiled;
        glGetShaderParam(object,GL_OBJECT_COMPILE_STATUS_ARB,&compiled);
        if(!compiled)
        {
            GLint log_len=0;
            log()<<"Can't compile "<<type_str[type]<<" shader: \n";

            glGetShaderParam(object,GL_OBJECT_INFO_LOG_LENGTH_ARB,&log_len);
            if(log_len>0)
            {
                std::string log_text(log_len,0);
                glGetInfoLogARB(object,log_len,&log_len,&log_text[0]);
                log()<<log_text.c_str()<<"\n";
            }

            shaders.remove(idx);
            return -1;
        }
        glAttachObjectARB(shdr.program,object);
        shdr.objects[type]=object;

#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
        if(i==0)
        {
            for(int i=0;i<parser.get_attributes_count();++i)
            {
                const shader_code_parser::variable a=parser.get_attribute(i);
                if(a.name=="_nya_Vertex") glBindAttribLocationARB(shdr.program,vertex_attribute,a.name.c_str());
                else if(a.name=="_nya_Normal") glBindAttribLocationARB(shdr.program,normal_attribute,a.name.c_str());
                else if(a.name=="_nya_Color") glBindAttribLocationARB(shdr.program,color_attribute,a.name.c_str());
                else if(a.name.find("_nya_MultiTexCoord")==0) glBindAttribLocationARB(shdr.program,tc0_attribute+a.idx,a.name.c_str());
            }
        }
#endif

        for(int j=0;j<parser.get_uniforms_count();++j)
        {
            const shader_code_parser::variable from=parser.get_uniform(j);
            shader_obj::uniform to;
            to.name=from.name;
            to.type=(shader::uniform_type)from.type;
            to.array_size=from.array_size;

            if(to.type==shader::uniform_mat4)
            {
                if(to.name=="_nya_ModelViewProjectionMatrix")
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    shdr.mat_mvp=1;
#endif
                    continue;
                }
                if(to.name=="_nya_ModelViewMatrix")
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    shdr.mat_mv=1;
#endif
                    continue;
                }
                if(to.name=="_nya_ProjectionMatrix")
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    shdr.mat_p=1;
#endif
                    continue;
                }
            }

            int idx=-1;
            for(int k=0;k<(int)shdr.uniforms.size();++k) { if(shdr.uniforms[k].name==from.name) { idx=k; break; } }
            if(idx<0) shdr.uniforms.push_back(to);
        }
    }

    GLint result=0;
    glLinkProgramARB(shdr.program);
    glGetObjectParam(shdr.program,GL_OBJECT_LINK_STATUS_ARB,&result);
    if(!result)
    {
        log()<<"Can't link shader\n";
        //TEMP DEBUG (iOS): dump the ES2-converted sources actually submitted to GL
        fprintf(stderr,"OH_SHADERLINK fail, converted vertex: %s\n===ENDV===\n", last_converted_code[0].c_str());
        fprintf(stderr,"OH_SHADERLINK fail, converted pixel: %s\n===ENDF===\n", last_converted_code[1].c_str());
        GLint log_len=0;
        glGetObjectParam(shdr.program,GL_OBJECT_INFO_LOG_LENGTH_ARB,&log_len);
        if(log_len>0)
        {
            std::string log_text(log_len,0);
            glGetInfoLogARB(shdr.program,log_len,&log_len,&log_text[0]);
            log()<<log_text.c_str()<<"\n";
            //TEMP DEBUG (iOS): info log vanishes in buffered stdout - mirror to stderr
            fprintf(stderr,"OH_SHADERLINK info: %s\n", log_text.c_str());
        }

        shaders.remove(idx);
        return -1;
    }

    set_shader(idx);

    for(size_t i=0,layer=0;i<shdr.uniforms.size();++i)
    {
        const shader_obj::uniform &u=shdr.uniforms[i];
        if(u.type!=shader::uniform_sampler2d && u.type!=shader::uniform_sampler_cube)
            continue;

        int handler=glGetUniformLocationARB(shdr.program,u.name.c_str());
        if(handler>=0)
            glUniform1iARB(handler,(int)layer);
        else
            log()<<"Unable to set shader sampler \'"<<u.name.c_str()<<"\': probably not found\n";

        ++layer;
    }

    set_shader(-1);
/*
    if(shaders_validation)
    {
        result=0;
        glValidateProgramARB(shdr.program);
        glGetObjectParam(shdr.program,GL_OBJECT_VALIDATE_STATUS_ARB,&result);
        if(!result)
        {
            log()<<"Can't validate shader\n";

            GLint log_len=0;
            glGetObjectParam(shdr.program,GL_OBJECT_INFO_LOG_LENGTH_ARB,&log_len);
            if(log_len>0)
            {
                std::string log_text(log_len,0);
                glGetInfoLogARB(shdr.program,log_len,&log_len,&log_text[0]);
                log()<<log_text.c_str()<<"\n";
            }
            //shdr.program=0; //??
            //return false;
        }
    }
*/


#if !defined OPENGL_ES || defined __ANDROID__ //some android and desktop vendors ignore the standart
    set_shader(idx);
#endif

#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
    set_shader(idx);

    if(shdr.mat_mvp>0)
        shdr.mat_mvp=glGetUniformLocationARB(shdr.program,"_nya_ModelViewProjectionMatrix");
    else if(shdr.mat_mv>0)
        shdr.mat_mv=glGetUniformLocationARB(shdr.program,"_nya_ModelViewMatrix");
    else if(shdr.mat_p>0)
        shdr.mat_p=glGetUniformLocationARB(shdr.program,"_nya_ProjectionMatrix");
#endif

    for(int i=0;i<(int)shdr.uniforms.size();++i)
        shdr.uniforms[i].handler=glGetUniformLocationARB(shdr.program,shdr.uniforms[i].name.c_str());

    int cache_size=0;
    for(int i=0;i<(int)shdr.uniforms.size();++i)
    {
        shader_obj::uniform &u=shdr.uniforms[i];
        if(u.type==shader::uniform_sampler2d || u.type==shader::uniform_sampler_cube)
            continue;
        u.cache_idx=cache_size;
        cache_size+=u.array_size*(u.type==shader::uniform_mat4?16:4);

        if(u.handler<0)
            u.type=shader::uniform_not_found;
    }
    shdr.uniform_cache.resize(cache_size);

    set_shader(-1);

    return idx;
}

render_opengl::uint render_opengl::get_uniforms_count(int shader) { return (int)shaders.get(shader).uniforms.size(); }
shader::uniform render_opengl::get_uniform(int shader,int idx) { return shaders.get(shader).uniforms[idx]; }

void render_opengl::remove_shader(int shader)
{
    if(applied_state.shader==shader)
    {
        glUseProgramObjectARB(0);
        applied_state.shader=-1;
    }
    shaders.remove(shader);
}

//ToDo: uniform buffers

int render_opengl::create_uniform_buffer(int shader) { return shader; }

void render_opengl::set_uniform(int shader,int idx,const float *buf,uint count)
{
    shader_obj &s=shaders.get(shader);

    float *cache=&s.uniform_cache[s.uniforms[idx].cache_idx];
    if(memcmp(cache,buf,count*sizeof(float))==0)
        return;
    memcpy(cache,buf,count*sizeof(float));

    set_shader(shader);

    const int handler=s.uniforms[idx].handler;
    switch(s.uniforms[idx].type)
    {
        case shader::uniform_mat4: glUniformMatrix4fvARB(handler,count/16,false,buf); break;
        case shader::uniform_vec4: glUniform4fvARB(handler,count/4,buf); break;
        case shader::uniform_vec3: glUniform3fvARB(handler,count/3,buf); break;
        case shader::uniform_vec2: glUniform2fvARB(handler,count/2,buf); break;
        case shader::uniform_float: glUniform1fvARB(handler,count,buf); break;
        default: break;
    }
}

void render_opengl::remove_uniform_buffer(int uniform_buffer) {}

//vbo

#if defined OPENGL3 || defined __APPLE__
    #define USE_VAO
#endif

#if !defined OPENGL_ES || defined __APPLE__
    #define USE_INSTANCING
#endif

#ifdef OPENGL_ES
    #define glGenVertexArrays glGenVertexArraysOES
    #define glBindVertexArray glBindVertexArrayOES
    #define glDeleteVertexArrays glDeleteVertexArraysOES
    #ifdef __APPLE__
        #define glDrawElementsInstancedARB glDrawElementsInstancedEXT
        #define glDrawArraysInstancedARB glDrawArraysInstancedEXT
    #endif
#elif defined __APPLE__ && !defined OPENGL3
    #define glGenVertexArrays glGenVertexArraysAPPLE
    #define glBindVertexArray glBindVertexArrayAPPLE
    #define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif

#ifdef OPENGL_ES
    #define GL_HALF_FLOAT GL_HALF_FLOAT_OES
    //A/B test: Apple's ES2-compat dispatch rejects 0x8B61 with INVALID_ENUM,
    //keep the _OES spelling here (attribute setup passes normalized=false for
    //floating-point types, which the ES2 validator also requires)
#endif

#ifndef GL_HALF_FLOAT
    #define GL_HALF_FLOAT GL_HALF_FLOAT_ARB
#endif

#ifndef NO_EXTENSIONS_INIT
    PFNGLGENBUFFERSARBPROC glGenBuffers=NULL;
    PFNGLBINDBUFFERARBPROC glBindBuffer=NULL;
    PFNGLBUFFERDATAARBPROC glBufferData=NULL;
    PFNGLBUFFERSUBDATAARBPROC glBufferSubData=NULL;
    PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubData=NULL;
    PFNGLDELETEBUFFERSARBPROC glDeleteBuffers=NULL;
    PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture=NULL;
    PFNGLDRAWELEMENTSINSTANCEDARBPROC glDrawElementsInstancedARB=NULL;
    PFNGLDRAWARRAYSINSTANCEDARBPROC glDrawArraysInstancedARB=NULL;

  #ifdef OPENGL3
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer=NULL;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray=NULL;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray=NULL;
  #endif

  #ifdef USE_VAO
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray=NULL;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays=NULL;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays=NULL;
  #endif
#endif

#ifndef GL_ARRAY_BUFFER
    #define GL_ARRAY_BUFFER GL_ARRAY_BUFFER_ARB
#endif

#ifndef GL_DYNAMIC_DRAW
    #define GL_DYNAMIC_DRAW GL_DYNAMIC_DRAW_ARB
#endif

#ifndef GL_STATIC_DRAW
    #define GL_STATIC_DRAW GL_STATIC_DRAW_ARB
#endif

#ifndef GL_STREAM_DRAW
    #define GL_STREAM_DRAW GL_STREAM_DRAW_ARB
#endif

#ifndef GL_ELEMENT_ARRAY_BUFFER
    #define GL_ELEMENT_ARRAY_BUFFER GL_ELEMENT_ARRAY_BUFFER_ARB
#endif

namespace
{
    int get_gl_element_type(vbo::element_type type)
    {
        switch(type)
        {
            case vbo::triangles: return GL_TRIANGLES;;
            case vbo::triangle_strip: return GL_TRIANGLE_STRIP;
            case vbo::points: return GL_POINTS;
            case vbo::lines: return GL_LINES;
            case vbo::line_strip: return GL_LINE_STRIP;
            default: return -1;
        }

        return -1;
    }

    int get_gl_element_type(vbo::vertex_atrib_type type)
    {
        switch(type)
        {
            case vbo::float16: return GL_HALF_FLOAT;
            case vbo::float32: return GL_FLOAT;
            case vbo::uint8: return GL_UNSIGNED_BYTE;
        }

        return GL_FLOAT;
    }

    //floating-point attribute types must not request normalized conversion
    //(GL_INVALID_ENUM on strict ES3 validators such as Apple's)
    bool gl_attrib_normalized(vbo::vertex_atrib_type type)
    {
        return type!=vbo::float16 && type!=vbo::float32;
    }

    int gl_usage(vbo::usage_hint usage)
    {
        switch(usage)
        {
            case vbo::static_draw: return GL_STATIC_DRAW;
            case vbo::dynamic_draw: return GL_DYNAMIC_DRAW;
            case vbo::stream_draw: return GL_STREAM_DRAW;
        }

        return GL_DYNAMIC_DRAW;
    }

    bool check_init_vbo()
    {
        static bool initialised=false;
        static bool failed=true;
        if(initialised)
            return !failed;

        //if(!has_extension("GL_ARB_vertex_buffer_object"))
        //    return false;

#ifndef NO_EXTENSIONS_INIT
        if(!(glGenBuffers=(PFNGLGENBUFFERSARBPROC)get_extension("glGenBuffersARB"))) return false;
        if(!(glBindBuffer=(PFNGLBINDBUFFERARBPROC)get_extension("glBindBufferARB"))) return false;
        if(!(glBufferData=(PFNGLBUFFERDATAARBPROC)get_extension("glBufferDataARB"))) return false;
        if(!(glBufferSubData=(PFNGLBUFFERSUBDATAARBPROC)get_extension("glBufferSubDataARB"))) return false;
        if(!(glGetBufferSubData=(PFNGLGETBUFFERSUBDATAARBPROC)get_extension("glGetBufferSubDataARB"))) return false;
        if(!(glDeleteBuffers=(PFNGLDELETEBUFFERSARBPROC)get_extension("glDeleteBuffersARB"))) return false;
        if(!(glClientActiveTexture=(PFNGLCLIENTACTIVETEXTUREARBPROC)get_extension("glClientActiveTextureARB"))) return false;

#ifdef OPENGL3
        if(!(glVertexAttribPointer=(PFNGLVERTEXATTRIBPOINTERPROC)get_extension("glVertexAttribPointer"))) return false;
        if(!(glEnableVertexAttribArray=(PFNGLENABLEVERTEXATTRIBARRAYPROC)get_extension("glEnableVertexAttribArray"))) return false;
        if(!(glDisableVertexAttribArray=(PFNGLDISABLEVERTEXATTRIBARRAYPROC)get_extension("glDisableVertexAttribArray"))) return false;
#endif

#ifdef USE_VAO
        if(!(glBindVertexArray=(PFNGLBINDVERTEXARRAYPROC)get_extension("glBindVertexArray"))) return false;
        if(!(glGenVertexArrays=(PFNGLGENVERTEXARRAYSPROC)get_extension("glGenVertexArrays"))) return false;
        if(!(glDeleteVertexArrays=(PFNGLDELETEVERTEXARRAYSPROC)get_extension("glDeleteVertexArrays"))) return false;
#endif

        if(has_extension("GL_ARB_draw_instanced"))
        {
            glDrawElementsInstancedARB=(PFNGLDRAWELEMENTSINSTANCEDARBPROC)get_extension("glDrawElementsInstancedARB");
            glDrawArraysInstancedARB=(PFNGLDRAWARRAYSINSTANCEDARBPROC)get_extension("glDrawArraysInstancedARB");
        }
#endif

        initialised=true,failed=false;
        return true;
    }

    struct vert_buf
    {
        unsigned int id;

#ifdef USE_VAO
        unsigned int vertex_array_object;
        unsigned int active_vao_ibuf;
#endif
        vbo::layout layout;
        unsigned int stride;
        unsigned int count;
        vbo::usage_hint usage;

    public:
        void release()
        {

#ifdef USE_VAO
            if(vertex_array_object)
                glDeleteVertexArrays(1,&vertex_array_object);
            vertex_array_object=0;
#endif
            if(id)
                glDeleteBuffers(1,&id);
            id=0;
        }
    };
    render_objects<vert_buf> vert_bufs;

    struct ind_buf
    {
        unsigned int id;
        vbo::index_size type;
        unsigned int count;
        vbo::usage_hint usage;

    public:
        void release()
        {
            if(id)
                glDeleteBuffers(1,&id);
            id=0;
        }
    };
    render_objects<ind_buf> ind_bufs;
}

int render_opengl::create_vertex_buffer(const void *data,uint stride,uint count,vbo::usage_hint usage)
{
    if(!check_init_vbo())
        return -1;

    const int idx=vert_bufs.add();
    vert_buf &v=vert_bufs.get(idx);
    glGenBuffers(1,&v.id);
    glBindBuffer(GL_ARRAY_BUFFER,v.id);
    glBufferData(GL_ARRAY_BUFFER,count*stride,data,gl_usage(usage));
    v.count=count;
    v.stride=stride;
#ifdef USE_VAO
    v.vertex_array_object=0;
    v.active_vao_ibuf=0;
#endif
    applied_state.vertex_buffer=-1;
    return idx;
}

void render_opengl::set_vertex_layout(int idx,vbo::layout layout)
{
    vert_buf &v=vert_bufs.get(idx);
    v.layout=layout;

    //the applied vertex-buffer tracking must be invalidated together with the
    //VAO deletion: a stale applied_state would make draw() skip the rebind
    //branch and submit vertices through stale/default attribute pointers
    applied_state.vertex_buffer=-1;

#ifdef USE_VAO
    if(v.vertex_array_object>0)
    {
        glDeleteVertexArrays(1,&v.vertex_array_object);
        v.vertex_array_object=0;
    }
#endif
}

void render_opengl::update_vertex_buffer(int idx,const void *data)
{
    vert_buf &v=vert_bufs.get(idx);
    //if(applied_state.vertex_buffer!=idx)
    {
#ifdef USE_VAO
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER,v.id);
        applied_state.vertex_buffer=-1;
    }
    glBufferData(GL_ARRAY_BUFFER,v.count*v.stride,0,gl_usage(v.usage)); //orphaning
    glBufferSubData(GL_ARRAY_BUFFER,0,v.count*v.stride,data);

#ifdef USE_VAO
    //if(applied_state.vertex_buffer!=idx)
        glBindBuffer(GL_ARRAY_BUFFER,0);
#endif
}

bool render_opengl::get_vertex_data(int idx,void *data)
{
    const vert_buf &v=vert_bufs.get(idx);
    //if(applied_state.vertex_buffer!=idx)
    {
#ifdef USE_VAO
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER,v.id);
        applied_state.vertex_buffer=-1;
    }

#ifdef OPENGL_ES
  #ifdef __ANDROID__
    //ToDo
    return false;
  #else
    //apple hasn't GL_READ_ONLY_OES
    const GLvoid *buf=glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES);
    if(!buf)
        return false;

    memcpy(data,buf,v.stride*v.count);
    if(!glUnmapBufferOES(GL_ARRAY_BUFFER))
        return false;
  #endif
#else
    glGetBufferSubData(GL_ARRAY_BUFFER,0,v.stride*v.count,data);
#endif

#ifdef USE_VAO
    //if(applied_state.vertex_buffer!=idx)
        glBindBuffer(GL_ARRAY_BUFFER,0);
#endif
    return true;
}

void render_opengl::remove_vertex_buffer(int idx)
{
    if(applied_state.vertex_buffer==idx)
    {
#ifdef USE_VAO
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER,0);
        applied_state.vertex_buffer=-1;
    }
    vert_bufs.remove(idx);
}

int render_opengl::create_index_buffer(const void *data,vbo::index_size size,uint indices_count,vbo::usage_hint usage)
{
    if(!check_init_vbo())
        return -1;

    const int idx=ind_bufs.add();
    ind_buf &i=ind_bufs.get(idx);

#ifdef USE_VAO
    glBindVertexArray(0);
    applied_state.vertex_buffer=-1;
#else
    applied_state.index_buffer=-1;
#endif

    glGenBuffers(1,&i.id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i.id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,size*indices_count,data,gl_usage(usage));
    i.type=size;
    i.count=indices_count;
    i.usage=usage;

#ifdef USE_VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
#endif
    return idx;
}

void render_opengl::update_index_buffer(int idx,const void *data)
{
    const ind_buf &i=ind_bufs.get(idx);

#ifdef USE_VAO
    glBindVertexArray(0);
    applied_state.vertex_buffer=-1;
#else
    applied_state.index_buffer=-1;
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i.id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,i.count*i.type,data,gl_usage(i.usage));

#ifdef USE_VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
#endif
}

bool render_opengl::get_index_data(int idx,void *data)
{
    const ind_buf &i=ind_bufs.get(idx);

#ifdef USE_VAO
    glBindVertexArray(0);
    applied_state.vertex_buffer=-1;
#else
    applied_state.index_buffer=-1;
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i.id);

#ifdef OPENGL_ES
  #ifdef __ANDROID__
    //ToDo
    return false;
  #else
    //apple hasn't GL_READ_ONLY_OES
    const GLvoid *buf=glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER,GL_WRITE_ONLY_OES);
    if(!buf)
        return false;

    memcpy(data,buf,i.type*i.count);
    if(!glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER))
        return false;
  #endif
#else
    glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,i.type*i.count,data);
#endif

#ifdef USE_VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
#endif
    return true;
}

void render_opengl::remove_index_buffer(int idx)
{
    if(applied_state.index_buffer==idx)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        applied_state.index_buffer=-1;
    }
    ind_bufs.remove(idx);
}

//texture

namespace
{
    const unsigned int cube_faces[]={GL_TEXTURE_CUBE_MAP_POSITIVE_X,GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                     GL_TEXTURE_CUBE_MAP_POSITIVE_Y,GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                     GL_TEXTURE_CUBE_MAP_POSITIVE_Z,GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
#ifndef NO_EXTENSIONS_INIT
    PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2D=0;
#endif

#ifdef OPENGL3
#ifdef GL_LUMINANCE
#undef GL_LUMINANCE
#endif
#define GL_LUMINANCE GL_RED
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#endif

#ifdef OPENGL_ES
#define MANUAL_MIPMAP_GENERATION

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

#ifndef GL_IMG_texture_compression_pvrtc
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0x8c00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG 0x8c01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8c02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8c03
#endif

#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278

#ifdef __APPLE__
#define GL_ETC1_RGB8_OES GL_COMPRESSED_RGB8_ETC2
#else
#define GL_ETC1_RGB8_OES 0x8D64
#define GL_RED_EXT 0x1903
#endif
#endif

#if !defined OPENGL_ES || defined __APPLE__
    #define USE_BGRA
#endif

    void gl_setup_filtration(int target,bool has_mips,texture::filter minif,texture::filter magnif,texture::filter mip)
    {
        glTexParameteri(target,GL_TEXTURE_MAG_FILTER,magnif==texture::filter_nearest?GL_NEAREST:GL_LINEAR);

        GLint filter;

        if(has_mips)
        {
            if(minif==texture::filter_nearest)
            {
                if(mip==texture::filter_nearest)
                    filter=GL_NEAREST_MIPMAP_NEAREST;
                else
                    filter=GL_NEAREST_MIPMAP_LINEAR;
            }
            else
            {
                if(mip==texture::filter_nearest)
                    filter=GL_LINEAR_MIPMAP_NEAREST;
                else
                    filter=GL_LINEAR_MIPMAP_LINEAR;
            }
        }
        else if(minif==texture::filter_nearest)
            filter=GL_NEAREST;
        else
            filter=GL_LINEAR;

        glTexParameteri(target,GL_TEXTURE_MIN_FILTER,filter);
    }

    void gl_generate_mips_pre(int gl_type)
    {
#if defined GL_GENERATE_MIPMAP && !defined OPENGL3
        glTexParameteri(gl_type,GL_GENERATE_MIPMAP,GL_TRUE);
#endif
    }

    void gl_generate_mips_post(int gl_type)
    {
#if defined GL_GENERATE_MIPMAP && !defined OPENGL3
        glTexParameteri(gl_type,GL_GENERATE_MIPMAP,GL_FALSE);
#else
#ifndef NO_EXTENSIONS_INIT
        static PFNGLGENERATEMIPMAPPROC glGenerateMipmap=NULL;
        if(!glGenerateMipmap)
            glGenerateMipmap=(PFNGLGENERATEMIPMAPPROC)get_extension("glGenerateMipmap");
#endif
        glGenerateMipmap(gl_type);
#endif
    }

    bool gl_get_format(texture::color_format format,unsigned int &source_format,unsigned int &gl_format,unsigned int &precision)
    {
        precision=GL_UNSIGNED_BYTE;
        switch(format)
        {
            case texture::color_rgb: source_format=gl_format=GL_RGB; break; //in es stored internally as rgba
            case texture::color_rgba: source_format=gl_format=GL_RGBA; break;
#ifdef USE_BGRA
            case texture::color_bgra: source_format=GL_RGBA; gl_format=GL_BGRA; break;
#endif
            case texture::greyscale: source_format=gl_format=GL_LUMINANCE; break;
#ifdef OPENGL_ES
            case texture::color_r32f: source_format=GL_RED_EXT; gl_format=GL_RED_EXT; precision=GL_FLOAT; break;
            //ES3 contexts require sized internal formats; the ES2 headers only
            //define the _OES variants, so the core values are spelled out here.
            //An ES3 context is always available on Apple platforms (and is what
            //the app requests), so the unsized ES2 spellings are not used.
            case texture::color_rgb32f: source_format=0x8815 /*GL_RGB32F*/; gl_format=GL_RGB; precision=GL_FLOAT; break;
            case texture::color_rgba32f: source_format=0x8814 /*GL_RGBA32F*/; gl_format=GL_RGBA; precision=GL_FLOAT; break;
            case texture::color_half_rgba: source_format=0x881A /*GL_RGBA16F*/; gl_format=GL_RGBA; precision=GL_FLOAT; break;

            case texture::depth16: source_format=0x81A5 /*GL_DEPTH_COMPONENT16*/; gl_format=GL_DEPTH_COMPONENT; precision=GL_UNSIGNED_SHORT; break;
            case texture::depth24: source_format=0x81A6 /*GL_DEPTH_COMPONENT24*/; gl_format=GL_DEPTH_COMPONENT; precision=GL_UNSIGNED_INT; break;
            case texture::depth32: source_format=0x81A6 /*GL_DEPTH_COMPONENT24 (no int 32 in ES3 core)*/; gl_format=GL_DEPTH_COMPONENT; precision=GL_UNSIGNED_INT; break;

            case texture::etc1: source_format=gl_format=GL_ETC1_RGB8_OES; break;
            case texture::etc2: source_format=gl_format=GL_COMPRESSED_RGB8_ETC2; break;
            case texture::etc2_a1: source_format=gl_format=GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2; break;
            case texture::etc2_eac: source_format=gl_format=GL_COMPRESSED_RGBA8_ETC2_EAC; break;

            case texture::pvr_rgb2b: source_format=gl_format=GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG; break;
            case texture::pvr_rgb4b: source_format=gl_format=GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG; break;
            case texture::pvr_rgba2b: source_format=gl_format=GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG; break;
            case texture::pvr_rgba4b: source_format=gl_format=GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG; break;
#else
#ifdef OPENGL3
            case texture::color_r32f: source_format=GL_R32F; gl_format=GL_RED; precision=GL_FLOAT; break;
            case texture::color_rgb32f: source_format=GL_RGB32F; gl_format=GL_RGB; precision=GL_FLOAT; break;
            case texture::color_rgba32f: source_format=GL_RGBA32F; gl_format=GL_RGBA; precision=GL_FLOAT; break;
#else
            case texture::color_r32f: source_format=GL_R32F; gl_format=GL_RED; precision=GL_FLOAT; break;
            case texture::color_rgb32f: source_format=GL_RGB32F_ARB; gl_format=GL_RGB; precision=GL_FLOAT; break;
            case texture::color_rgba32f: source_format=GL_RGBA32F_ARB; gl_format=GL_RGBA; precision=GL_FLOAT; break;
#endif
            case texture::depth16: source_format=GL_DEPTH_COMPONENT16; gl_format=GL_DEPTH_COMPONENT; break;
            case texture::depth24: source_format=GL_DEPTH_COMPONENT24; gl_format=GL_DEPTH_COMPONENT; break;
            case texture::depth32: source_format=GL_DEPTH_COMPONENT32; gl_format=GL_DEPTH_COMPONENT; break;
#endif
            case texture::dxt1: source_format=gl_format=GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
            case texture::dxt3: source_format=gl_format=GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
            case texture::dxt5: source_format=gl_format=GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;

#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
    #define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93D0
    #define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93D2
    #define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93D4
#endif
            case texture::astc4x4: source_format=gl_format=GL_COMPRESSED_RGBA_ASTC_4x4_KHR; break;
            case texture::astc5x5: source_format=gl_format=GL_COMPRESSED_RGBA_ASTC_5x5_KHR; break;
            case texture::astc6x6: source_format=gl_format=GL_COMPRESSED_RGBA_ASTC_6x6_KHR; break;

            default: return false;
        };

        return true;
    }

    struct tex_obj
    {
        unsigned int tex_id,gl_type,gl_format,gl_precision;
        bool has_mip;

    public:
        void release()
        {
            if(tex_id)
                glDeleteTextures(1,&tex_id);
            tex_id=0;
        }
    };
    render_objects<tex_obj> textures;

    void set_texture(int idx,int layer)
    {
        gl_select_multitex_layer(layer);

        const int prev_tex=applied_state.textures[layer];
        applied_state.textures[layer]=idx;
        if(idx<0)
        {
            if(prev_tex>=0)
                glBindTexture(textures.get(prev_tex).gl_type,0);
            return;
        }

        const tex_obj &t=textures.get(idx);
        glBindTexture(t.gl_type,t.tex_id);
    }

    int create_texture_(const void *data_a[6],bool is_cubemap,unsigned int width,unsigned int height,texture::color_format &format,int mip_count)
    {
        const int idx=textures.add();
        tex_obj &t=textures.get(idx);
        glGenTextures(1,&t.tex_id);
        t.gl_type=is_cubemap?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
        set_texture(idx,0);

        const unsigned int source_bpp=texture::get_format_bpp(format);
#ifdef MANUAL_MIPMAP_GENERATION
        const bool bad_alignment=(source_bpp/8)%4!=0;
#else
        const bool bad_alignment=(width*source_bpp/8)%4!=0;
#endif
        if(bad_alignment)
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);

        t.has_mip=(mip_count>1 || mip_count<0);

        if(t.gl_type==GL_TEXTURE_CUBE_MAP)
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
#ifndef OPENGL_ES
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
#endif
        }

        const bool is_pvrtc=format==texture::pvr_rgb2b || format==texture::pvr_rgba2b || format==texture::pvr_rgb4b || format==texture::pvr_rgba4b;
        if(is_pvrtc)
            gl_setup_filtration(t.gl_type,t.has_mip,texture::filter_linear,texture::filter_linear,texture::filter_nearest);

#ifdef OPENGL3
        if(format==texture::greyscale)
        {
            const int swizzle[]={GL_RED,GL_RED,GL_RED,GL_ONE};
            glTexParameteriv(t.gl_type,GL_TEXTURE_SWIZZLE_RGBA,swizzle);
        }
#endif

#ifndef OPENGL_ES
        if(mip_count>1) //is_platform_restrictions_ignored() &&
            glTexParameteri(t.gl_type,GL_TEXTURE_MAX_LEVEL,mip_count-1);
#endif

#ifndef MANUAL_MIPMAP_GENERATION
        if(t.has_mip && mip_count<0)
            gl_generate_mips_pre(t.gl_type);
#endif
        unsigned int source_format=0;
        gl_get_format(format,source_format,t.gl_format,t.gl_precision);

        for(int j=0;j<(is_cubemap?6:1);++j)
        {
            const unsigned char *data_pointer=data_a?(const unsigned char*)data_a[j]:0;
            unsigned int w=width,h=height;
            const int gl_type=is_cubemap?cube_faces[j]:GL_TEXTURE_2D;

#ifdef MANUAL_MIPMAP_GENERATION
            if(mip_count<0 && data_pointer)
            {
                //double-buffer the downsample: bitmap_downsample2x does not
                //support in-place operation (it writes rows top-down, which
                //would overwrite source rows not yet read). Alternating
                //between two scratch buffers keeps src and dst distinct on
                //every level after the first. Single-buffer (the old code)
                //corrupted mip levels 2+ on hardware rasterizers, showing as
                //red/garbage polygons on distant geometry.
                nya_memory::tmp_buffer_scoped buf0(w*h*(source_bpp/8)/2);
                nya_memory::tmp_buffer_scoped buf1(w*h*(source_bpp/8)/2);
                unsigned char *dst=(unsigned char *)buf0.get_data();
                unsigned char *alt=(unsigned char *)buf1.get_data();
                for(int i=0;;++i,w=w>1?w/2:1,h=h>1?h/2:1)
                {
                    glTexImage2D(gl_type,i,source_format,w,h,0,t.gl_format,t.gl_precision,data_pointer);
                    if(w==1 && h==1)
                        break;

                    bitmap_downsample2x(data_pointer,w,h,source_bpp/8,dst);
                    data_pointer=dst;
                    std::swap(dst,alt);
                }
            }
            else
#endif
            {
                for(int i=0;i<(mip_count<=0?1:mip_count);++i,w=w>1?w/2:1,h=h>1?h/2:1)
                {
                    unsigned int size=0;
                    if(format<texture::dxt1)
                    {
                        size=w*h*(source_bpp/8);
                        glTexImage2D(gl_type,i,source_format,w,h,0,t.gl_format,t.gl_precision,data_pointer);
                    }
                    else
                    {
                        if(is_pvrtc)
                        {
                            if(format==texture::pvr_rgb2b || format==texture::pvr_rgba2b)
                                size=((w>16?w:16)*(h>8?h:8)*2 + 7)/8;
                            else
                                size=((w>8?w:8)*(h>8?h:8)*4 + 7)/8;

                            if(size<32)
                                break;
                        }
                        else if(format==texture::astc4x4 || format==texture::astc5x5 || format==texture::astc6x6)
                        {
                            const unsigned int b=(format==texture::astc6x6)?6:(format==texture::astc5x5?5:4);
                            size=((w+b-1)/b) * ((h+b-1)/b) * 16;
                        }
                        else
                            size=(w>4?w:4)/4 * (h>4?h:4)/4 * source_bpp*2;

#ifndef NO_EXTENSIONS_INIT
                        if(!glCompressedTexImage2D)
                            glCompressedTexImage2D=(PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)get_extension("glCompressedTexImage2DARB");
                        if(glCompressedTexImage2D)
#endif
                            glCompressedTexImage2D(gl_type,i,t.gl_format,w,h,0,size,data_pointer);
                    }

                    data_pointer+=size;
                }
            }
        }

        if(bad_alignment)
            glPixelStorei(GL_UNPACK_ALIGNMENT,4);

        //mip_count<0 means "generate mipmaps automatically". Under
        //MANUAL_MIPMAP_GENERATION the gl_generate_mips_pre path is suppressed,
        //so a mip_count<0 texture uploaded with only its base level would have
        //has_mip=true but no mip data, causing minification flicker. Call
        //glGenerateMipmap explicitly, but ONLY for uncompressed formats
        //(glGenerateMipmap is undefined for compressed textures in ES3 and
        //produces garbage/red mip levels on Apple's driver).
        if(t.has_mip && mip_count<0 && format<texture::dxt1)
            gl_generate_mips_post(t.gl_type);

#ifdef OPENGL_ES
        if(t.gl_format==GL_RGB)
            t.gl_format=GL_RGBA;
#endif
        return idx;
    }
}

int render_opengl::create_texture(const void *data,uint width,uint height,texture::color_format &format,int mip_count)
{
    const void *data_a[6]={data};
    return create_texture_(data_a,false,width,height,format,mip_count);
}

int render_opengl::create_cubemap(const void *data[6],uint width,texture::color_format &format,int mip_count)
{
    return create_texture_(data,true,width,width,format,mip_count);
}

void render_opengl::update_texture(int idx,const void *data,uint x,uint y,uint width,uint height,int mip)
{
    const tex_obj &t=textures.get(idx);
    set_texture(idx,0);

#ifdef __ANDROID__ //adreno 4xx fix
    glFinish();
#endif

    if(t.has_mip && mip<0)
        gl_generate_mips_pre(t.gl_type);

    glTexSubImage2D(t.gl_type,mip<0?0:mip,x,y,width,height,t.gl_format,t.gl_precision,data);

    if(t.has_mip && mip<0)
    {
        gl_generate_mips_post(t.gl_type);

    #if defined __APPLE__ && defined OPENGL_ES //ios devices don't wait for mip generation
        glFinish();
    #endif
    }
}

void render_opengl::set_texture_wrap(int idx,texture::wrap s,texture::wrap t)
{
    const tex_obj &tex=textures.get(idx);
    set_texture(idx,0);

    const texture::wrap wraps[]={s,t};
    const GLint pnames[]={GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T};
    for(int i=0;i<2;++i)
    {
        switch(wraps[i])
        {
            case texture::wrap_clamp:glTexParameteri(tex.gl_type,pnames[i],GL_CLAMP_TO_EDGE);break;
            case texture::wrap_repeat:glTexParameteri(tex.gl_type,pnames[i],GL_REPEAT);break;
            case texture::wrap_repeat_mirror:glTexParameteri(tex.gl_type,pnames[i],GL_MIRRORED_REPEAT);break;
        }
    }
}

void render_opengl::set_texture_filter(int idx,texture::filter minification,texture::filter magnification,texture::filter mipmap,uint aniso)
{
    static int max_aniso = -1;
    if(max_aniso<0)
    {
        GLfloat f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &f);
        max_aniso=int(f);
    }

    if((int)aniso>max_aniso)
        aniso=(uint)max_aniso;

    const tex_obj &t=textures.get(idx);
    set_texture(idx,0);
    glTexParameterf(t.gl_type,GL_TEXTURE_MAX_ANISOTROPY_EXT,float(aniso));
    gl_setup_filtration(t.gl_type,t.has_mip,minification,magnification,mipmap);
}

namespace
{
#ifndef NO_EXTENSIONS_INIT
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;

    PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
    PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
    PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
    PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
#endif
}

bool render_opengl::get_texture_data(int texture,uint x,uint y,uint w,uint h,void *data)
{
    const tex_obj &t=textures.get(texture);

    //compressed formats are not supported
    switch(t.gl_format)
    {
#ifdef OPENGL_ES
        case GL_ETC1_RGB8_OES:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_RGBA8_ETC2_EAC:
        case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
        case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
        case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
        case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
#endif
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return false;
    }

    if(!check_init_fbo())
        return false;

#ifdef __ANDROID__ //adreno 4xx fix
    glFinish();
#endif

    glViewport(0,0,x+w,y+h);

    uint tmp_fbo;
    glGenFramebuffers(1,&tmp_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,tmp_fbo);

    if(t.gl_type==GL_TEXTURE_CUBE_MAP)
    {
        unsigned int size=(w-x)*(h-y);
        if(t.gl_precision==GL_FLOAT)
            size*=4;

        switch(t.gl_format)
        {
            case GL_RGB: size*=3; break;

            case GL_RGBA: 
#ifdef USE_BGRA
			case GL_BGRA:
#endif
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                size *=4;
                break;
        }

        for(int i=0;i<6;++i)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,gl_cube_type(i),t.tex_id,0);
            glReadPixels(x,y,w,h,t.gl_format,t.gl_precision,(char *)data+size*i);
        }
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,t.gl_type,t.tex_id,0);
        glReadPixels(x,y,w,h,t.gl_format,t.gl_precision,data);
    }

    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(1,&tmp_fbo);

    applied_state.target= -1;
    glViewport(applied_state.viewport.x,applied_state.viewport.y,applied_state.viewport.width,applied_state.viewport.height);
    return true;
}

void render_opengl::remove_texture(int idx)
{
    for(int i=0;i<state::max_layers;++i)
    {
        if(applied_state.textures[i]!=idx)
            continue;

        applied_state.textures[i]=-1;
        gl_select_multitex_layer(i);
        const tex_obj &t=textures.get(idx);
        glBindTexture(t.gl_type,0);
    }
    textures.remove(idx);
}

unsigned int render_opengl::get_max_texture_dimention()
{
    static unsigned int max_tex_size=0;
    if(!max_tex_size)
    {
        GLint texSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);
        max_tex_size=texSize;
    }

    return max_tex_size;
}

bool render_opengl::is_texture_format_supported(texture::color_format format)
{
    unsigned int source_format,gl_format,precision;
    if(!gl_get_format(format,source_format,gl_format,precision))
        return false;

    //ASTC needs a real capability check: the mapping above always succeeds,
    //but desktop GL (macOS) has no ASTC support and would fail at upload time
    if(format==texture::astc4x4 || format==texture::astc5x5 || format==texture::astc6x6)
    {
        static int astc_supported=-1;
        if(astc_supported<0)
        {
            astc_supported=0;
            const char *exts=(const char*)glGetString(GL_EXTENSIONS);
            if(exts && strstr(exts,"texture_compression_astc"))
                astc_supported=1;
        }

        return astc_supported!=0;
    }

    //S3TC/DXT likewise: the mapping always succeeds, but Apple GLES has no
    //S3TC at all (ES2 and ES3) - CPU-decode fallback must trigger there
    if(format==texture::dxt1 || format==texture::dxt3 || format==texture::dxt5)
    {
        static int dxt_supported=-1;
        if(dxt_supported<0)
        {
            dxt_supported=0;
            const char *exts=(const char*)glGetString(GL_EXTENSIONS);
            if(exts && (strstr(exts,"texture_compression_s3tc")
                        || strstr(exts,"WEBGL_compressed_texture_s3tc")))
                dxt_supported=1;
        }

        return dxt_supported!=0;
    }

    //float color attachments must be renderable: 32F needs
    //EXT_color_buffer_float, 16F needs EXT_color_buffer_half_float.
    //Apple's simulator GLES only exposes the half-float one, so 32F targets
    //degrade to RGBA16F in build_texture() instead of rendering garbage.
    if(format==texture::color_rgb32f || format==texture::color_rgba32f
       || format==texture::color_r32f)
    {
        static int float32_supported=-1;
        if(float32_supported<0)
        {
            float32_supported=0;
            const char *exts=(const char*)glGetString(GL_EXTENSIONS);
            if(exts && strstr(exts,"EXT_color_buffer_float"))
                float32_supported=1;
        }

        return float32_supported!=0;
    }
    if(format==texture::color_half_rgba)
    {
        static int half_supported=-1;
        if(half_supported<0)
        {
            half_supported=0;
            const char *exts=(const char*)glGetString(GL_EXTENSIONS);
            if(exts && (strstr(exts,"EXT_color_buffer_half_float")
                        || strstr(exts,"EXT_color_buffer_float")))
                half_supported=1;
        }

        return half_supported!=0;
    }

    return true;
}

//target

namespace
{
    int default_fbo_idx=0;

#ifndef GL_MULTISAMPLE
    #define GL_MULTISAMPLE GL_MULTISAMPLE_ARB
#endif

#ifdef OPENGL_ES
    #ifdef __APPLE__
        #define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleAPPLE
    #else
        typedef void (*PFNGLBLITFRAMEBUFFERPROC)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
        typedef void (*PFNGLREADBUFFERPROC) (GLenum);
        typedef void (*PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum,GLsizei,GLenum,GLsizei,GLsizei);
        PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer=0;
        PFNGLREADBUFFERPROC glReadBuffer=0;
        PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample=0;
        #define GL_READ_FRAMEBUFFER 0x8CA8
        #define GL_DRAW_FRAMEBUFFER 0x8CA9
    #endif
#endif

    bool check_init_fbo()
    {
        static bool initialised=false;
        static bool failed=true;
        if(initialised)
            return !failed;

        //if(!has_extension("GL_EXT_framebuffer_object"))
        //    return false;

#ifndef NO_EXTENSIONS_INIT
        if(!(glGenFramebuffers=(PFNGLGENFRAMEBUFFERSPROC)get_extension("glGenFramebuffers"))) return false;
        if(!(glBindFramebuffer=(PFNGLBINDFRAMEBUFFERPROC)get_extension("glBindFramebuffer"))) return false;
        if(!(glDeleteFramebuffers=(PFNGLDELETEFRAMEBUFFERSPROC)get_extension("glDeleteFramebuffers"))) return false;
        if(!(glFramebufferTexture2D=(PFNGLFRAMEBUFFERTEXTURE2DPROC)get_extension("glFramebufferTexture2D"))) return false;

        //multisample
        glBlitFramebuffer=(PFNGLBLITFRAMEBUFFERPROC)get_extension("glBlitFramebuffer");
        glGenRenderbuffers=(PFNGLGENRENDERBUFFERSPROC)get_extension("glGenRenderbuffers");
        glBindRenderbuffer=(PFNGLBINDRENDERBUFFERPROC)get_extension("glBindRenderbuffer");
        glDeleteRenderbuffers=(PFNGLDELETERENDERBUFFERSPROC)get_extension("glDeleteRenderbuffers");
        glRenderbufferStorageMultisample=(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)get_extension("glRenderbufferStorageMultisample");
        glFramebufferRenderbuffer=(PFNGLFRAMEBUFFERRENDERBUFFERPROC)get_extension("glFramebufferRenderbuffer");
#endif

#if defined OPENGL_ES && !defined __APPLE__
        const char *gl_version=(const char *)glGetString(GL_VERSION);
        const bool es3=gl_version!=0 && strncmp(gl_version,"OpenGL ES 3.",12)==0;
        if(es3)
        {
            glBlitFramebuffer=(PFNGLBLITFRAMEBUFFERPROC)get_extension("glBlitFramebuffer");
            glReadBuffer=(PFNGLREADBUFFERPROC)get_extension("glReadBuffer");
            glRenderbufferStorageMultisample=(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)get_extension("glRenderbufferStorageMultisample");
        }
#endif

        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&default_fbo_idx);
        initialised=true,failed=false;
        return true;
    }

#ifndef GL_MAX_COLOR_ATTACHMENTS
    #define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#endif

#ifndef GL_MAX_SAMPLES
    #define GL_MAX_SAMPLES 0x8D57
#endif

    struct ms_buffer
    {
        unsigned int fbo,buf;
        unsigned int width,height,samples,format;

        void create(unsigned int w,unsigned int h,unsigned int f,unsigned int s)
        {
            if(!glRenderbufferStorageMultisample)
                return;

            if(!w || !h || s<1)
            {
                release();
                return;
            }

            if(w==width && h==height && s==samples && f==format)
                return;

#ifdef OPENGL_ES
            if(f==GL_RGBA)
                f=GL_RGB5_A1;
#endif
            if(!buf)
                glGenRenderbuffers(1,&buf);

            glBindRenderbuffer(GL_RENDERBUFFER,buf);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,s,f,w,h);
            glBindRenderbuffer(GL_RENDERBUFFER,0);
            glGenFramebuffers(1,&fbo);

            width=w,height=h,samples=s,format=f;
        }

        void resolve(int from,tex_obj &tex,int cubemap_side,int attachment_idx) const;

        void release()
        {
            if(buf) glDeleteRenderbuffers(1,&buf);
            if(fbo) glDeleteFramebuffers(1,&fbo);
            *this=ms_buffer();
        }

        ms_buffer(): buf(0),width(0),height(0),samples(0),fbo(0) {}
    };

    struct fbo_obj
    {
        struct attachment
        {
            int tex_idx,cubemap_side;
            ms_buffer multisample;

            attachment(): tex_idx(-1),cubemap_side(-1) {}
        };

        std::vector<attachment> color_attachments;
        int depth_tex_idx;
        unsigned int id;
        int depth_target_idx;

        ms_buffer multisample_depth;
        bool depth_only;

        void release()
        {
            for(size_t i=0;i<color_attachments.size();++i)
                color_attachments[i].multisample.release();
            multisample_depth.release();
            if(id)
                glDeleteFramebuffers(1,&id);
            *this=fbo_obj();
        }
    };
    render_objects<fbo_obj> fbos;

    void ms_buffer::resolve(int from,tex_obj &tex,int cubemap_side,int attachment_idx) const
    {
        const int gl_type=cubemap_side<0?tex.gl_type:gl_cube_type(cubemap_side);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,gl_type,tex.tex_id,0);
        glBindFramebuffer(GL_FRAMEBUFFER,default_fbo_idx);

#if defined OPENGL_ES && defined __APPLE__
        if(attachment_idx!=0) //ToDo
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE,from);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE,fbo);
        glResolveMultisampleFramebufferAPPLE();
        glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE,default_fbo_idx);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE,default_fbo_idx);
#else
    #ifdef OPENGL_ES
        if(glBindFramebuffer)
    #endif
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER,from);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0+attachment_idx);
            glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,default_fbo_idx);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,default_fbo_idx);
        }
#endif
    }
}

int render_opengl::create_target(uint width,uint height,uint samples,const int *attachment_textures,
                                 const int *attachment_sides,uint attachment_count,int depth_texture)
{
    int idx=fbos.add();
    fbo_obj &f=fbos.get(idx);

    glGenFramebuffers(1,&f.id);
    glBindFramebuffer(GL_FRAMEBUFFER,f.id);

    bool has_color=false;
    f.color_attachments.resize(attachment_count);
    for(uint i=0;i<attachment_count;++i)
    {
        if(attachment_textures[i]<0)
            continue;

        f.color_attachments[i].tex_idx=attachment_textures[i];
        f.color_attachments[i].cubemap_side=attachment_sides[i];
        has_color=true;

        const tex_obj &t=textures.get(attachment_textures[i]);
        if(samples>1)
        {
            f.color_attachments[i].multisample.create(width,height,t.gl_format,samples);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_RENDERBUFFER,f.color_attachments[i].multisample.buf);
        }
        else
        {
            const int gl_type=attachment_sides[i]<0?t.gl_type:gl_cube_type(attachment_sides[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,gl_type,t.tex_id,0);
        }
    }

    if(depth_texture>=0)
    {
        const tex_obj &t=textures.get(depth_texture);
        if(samples>1)
        {
            f.multisample_depth.create(width,height,t.gl_format,samples);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,f.multisample_depth.buf);
        }
        else
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,t.tex_id,0);
        f.depth_target_idx=t.tex_id;
    }

    f.depth_only=!has_color && depth_texture>=0;

    glBindFramebuffer(GL_FRAMEBUFFER,applied_state.target>=0?fbos.get(applied_state.target).id:default_fbo_idx);
    return idx;
}

void render_opengl::resolve_target(int idx)
{
    const fbo_obj &f=fbos.get(idx);
    for(int i=0;i<(int)f.color_attachments.size();++i)
    {
        const fbo_obj::attachment &a=f.color_attachments[i];
        if(a.tex_idx<0)
            continue;

        tex_obj &tex=textures.get(a.tex_idx);
        a.multisample.resolve(f.id,tex,a.cubemap_side,int(i));

        if(tex.has_mip)
        {
            gl_select_multitex_layer(0);
            glBindTexture(tex.gl_type, tex.tex_id);
            applied_state.textures[0]=tex.tex_id;

#ifndef NO_EXTENSIONS_INIT
            static PFNGLGENERATEMIPMAPPROC glGenerateMipmap=NULL;
            if(!glGenerateMipmap)
                glGenerateMipmap=(PFNGLGENERATEMIPMAPPROC)get_extension("glGenerateMipmap");
#endif
            glGenerateMipmap(tex.gl_type);
        }
    }
}

void render_opengl::remove_target(int idx)
{
    if(applied_state.target==idx)
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        applied_state.target=-1;
    }
    return fbos.remove(idx);
}

unsigned int render_opengl::get_max_target_attachments()
{
    static int max_attachments= -1;
    if(max_attachments<0)
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_attachments);
    if(max_attachments<0)
        max_attachments=1;

    return max_attachments;
}

unsigned int render_opengl::get_max_target_msaa()
{
    static int max_ms=-1;

#if defined OPENGL_ES && !defined __APPLE__
    if (!check_init_fbo())
        return 0;

    if (!glBlitFramebuffer || !glReadBuffer || !glRenderbufferStorageMultisample)
        return 1;
#endif

    if(max_ms<0)
        glGetIntegerv(GL_MAX_SAMPLES,&max_ms);

    return max_ms>1?max_ms:1;
}

//general

static void apply_viewport_state(render_api_interface::viewport_state s)
{
    if(s.target!=applied_state.target || ignore_cache_vp)
    {
        if(check_init_fbo())
        {
            glBindFramebuffer(GL_FRAMEBUFFER,s.target>=0?fbos.get(s.target).id:default_fbo_idx);

    #ifndef OPENGL_ES
            const bool no_color=s.target>=0 && fbos.get(s.target).depth_only;
            if(no_color!=was_fbo_without_color || ignore_cache_vp)
            {
                const int buffer=no_color?GL_NONE:GL_COLOR_ATTACHMENT0;
                glDrawBuffer(buffer);
                glReadBuffer(buffer);
                was_fbo_without_color=no_color;
            }
    #endif
        }
    }

    if(applied_state.viewport!=s.viewport || ignore_cache_vp)
        glViewport(s.viewport.x,s.viewport.y,s.viewport.width,s.viewport.height);

    if(memcmp(applied_state.clear_color,s.clear_color,sizeof(applied_state.clear_color))!=0 || ignore_cache_vp)
        glClearColor(s.clear_color[0],s.clear_color[1],s.clear_color[2],s.clear_color[3]);

    if(applied_state.clear_depth!=s.clear_depth || ignore_cache_vp)
    {
#ifdef OPENGL_ES
        glClearDepthf(s.clear_depth);
#else
        glClearDepth(s.clear_depth);
#endif
    }

    if(s.scissor_enabled!=applied_state.scissor_enabled || ignore_cache_vp)
    {
        if(s.scissor_enabled)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);
    }

    if(s.scissor!=applied_state.scissor)
        glScissor(s.scissor.x,s.scissor.y,s.scissor.width,s.scissor.height);

    *(render_api_interface::viewport_state*)&applied_state=s;
	ignore_cache_vp=false;
}

inline GLenum gl_blend_mode(blend::mode m)
{
    switch(m)
    {
        case blend::zero: return GL_ZERO;
        case blend::one: return GL_ONE;
        case blend::src_color: return GL_SRC_COLOR;
        case blend::inv_src_color: return GL_ONE_MINUS_SRC_COLOR;
        case blend::src_alpha: return GL_SRC_ALPHA;
        case blend::inv_src_alpha: return GL_ONE_MINUS_SRC_ALPHA;
        case blend::dst_color: return GL_DST_COLOR;
        case blend::inv_dst_color: return GL_ONE_MINUS_DST_COLOR;
        case blend::dst_alpha: return GL_DST_ALPHA;
        case blend::inv_dst_alpha: return GL_ONE_MINUS_DST_ALPHA;
    }

    return GL_ONE;
}

void render_opengl::clear(const viewport_state &s,bool color,bool depth,bool stencil)
{
    apply_viewport_state(s);

    GLbitfield mode=0;
    if(color)
    {
        mode|=GL_COLOR_BUFFER_BIT;
        if(!applied_state.color_write)
        {
            glColorMask(true,true,true,true);
            applied_state.color_write=true;
        }
    }

    if(depth)
    {
        mode|=GL_DEPTH_BUFFER_BIT;
        if(!applied_state.zwrite)
        {
            glDepthMask(true);
            applied_state.zwrite=true;
        }
    }

    if(stencil)
        mode|=GL_STENCIL_BUFFER_BIT;

    glClear(mode);
}

void render_opengl::invalidate_cached_state()
{
    ignore_cache = true;
	ignore_cache_vp = true;

    applied_state.index_buffer=applied_state.vertex_buffer= -1;
    applied_state.shader=applied_state.uniform_buffer= -1;
    active_layer=-1;
    for(int i=0;i<state::max_layers;++i)
    {
        applied_state.textures[i]=-1;
        gl_select_multitex_layer(i);
        glBindTexture(GL_TEXTURE_2D,0);
        glBindTexture(GL_TEXTURE_CUBE_MAP,0);
    }

#ifndef USE_VAO
    applied_layout=vbo::layout();
    //ToDo: disable layout client states
#endif
}

void render_opengl::apply_state(const state &c)
{
    state &a=applied_state;

    apply_viewport_state(c);

#ifndef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(c.projection[0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(c.modelview[0]);
#endif

    if(c.blend!=a.blend || ignore_cache)
    {
        if(c.blend)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        a.blend=c.blend;
    }

    if(c.blend_src!=a.blend_src || c.blend_dst!=a.blend_dst || ignore_cache)
    {
#ifndef NO_EXTENSIONS_INIT
        static bool initialised=false;
        if(!initialised)
        {
            glBlendFuncSeparate=(PFNGLBLENDFUNCSEPARATEPROC)get_extension("glBlendFuncSeparate");
            initialised=true;
        }
#endif
        glBlendFuncSeparate(gl_blend_mode(c.blend_src),gl_blend_mode(c.blend_dst),GL_ONE,GL_ONE);
        a.blend_src=c.blend_src,a.blend_dst=c.blend_dst;
    }

    if(c.cull_face!=a.cull_face || ignore_cache)
    {
        if(c.cull_face)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        a.cull_face=c.cull_face;
    }

    if(c.cull_order!=a.cull_order || ignore_cache)
    {
        if(c.cull_order==cull_face::cw)
            glFrontFace(GL_CW);
        else
            glFrontFace(GL_CCW);
        a.cull_order=c.cull_order;
    }

    if(c.depth_test!=a.depth_test || ignore_cache)
    {
        if(c.depth_test)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        a.depth_test=c.depth_test;
    }

    if(c.depth_comparsion!=a.depth_comparsion || ignore_cache)
    {
        switch(c.depth_comparsion)
        {
            case depth_test::never: glDepthFunc(GL_NEVER); break;
            case depth_test::less: glDepthFunc(GL_LESS); break;
            case depth_test::equal: glDepthFunc(GL_EQUAL); break;
            case depth_test::greater: glDepthFunc(GL_GREATER); break;
            case depth_test::not_less: glDepthFunc(GL_GEQUAL); break;
            case depth_test::not_equal: glDepthFunc(GL_NOTEQUAL); break;
            case depth_test::not_greater: glDepthFunc(GL_LEQUAL); break;
            case depth_test::allways: glDepthFunc(GL_ALWAYS); break;
        }
        a.depth_comparsion=c.depth_comparsion;
    }

    if(c.zwrite!=a.zwrite || ignore_cache)
    {
        glDepthMask(c.zwrite);
        a.zwrite=c.zwrite;
    }

    if(c.color_write!=a.color_write || ignore_cache)
    {
        glColorMask(c.color_write,c.color_write,c.color_write,c.color_write);
        a.color_write=c.color_write;
    }

    for(int i=0;i<c.max_layers;++i)
    {
        if(c.textures[i]!=a.textures[i])
            set_texture(c.textures[i],i);
    }

    ignore_cache = false;
}

void render_opengl::draw(const state &s)
{
    if(s.vertex_buffer<0 || s.shader<0)
        return;

    apply_state(s);
    set_shader(s.shader);

#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
    shader_obj &shdr=shaders.get(s.shader);
    if(shdr.mat_mvp>=0)
    {
        static float mvp[4][4];
        s.get_mvp(mvp);
        glUniformMatrix4fvARB(shdr.mat_mvp,1,false,mvp[0]);
    }
    if(shdr.mat_mv>=0)
        glUniformMatrix4fvARB(shdr.mat_mv,1,false,s.modelview[0]);
    if(shdr.mat_p>=0)
        glUniformMatrix4fvARB(shdr.mat_p,1,false,s.projection[0]);
#endif

    vert_buf &v=vert_bufs.get(s.vertex_buffer);

    if(s.vertex_buffer!=applied_state.vertex_buffer)
    {
#ifdef USE_VAO
        if(v.vertex_array_object>0)
        {
            glBindVertexArray(v.vertex_array_object);
        }
        else
        {
            glGenVertexArrays(1,&v.vertex_array_object);
            glBindVertexArray(v.vertex_array_object);

            applied_layout=vbo::layout();
            v.active_vao_ibuf=0;
#endif
            glBindBuffer(GL_ARRAY_BUFFER,v.id);

#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
            glEnableVertexAttribArray(vertex_attribute);
            glVertexAttribPointer(vertex_attribute,v.layout.pos.dimension,get_gl_element_type(v.layout.pos.type),gl_attrib_normalized(v.layout.pos.type),
                                  v.stride,(void*)(ptrdiff_t)(v.layout.pos.offset));
#else
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(v.layout.pos.dimension,get_gl_element_type(v.layout.pos.type),v.stride,(void*)0);
#endif
            for(unsigned int i=0;i<vbo::max_tex_coord;++i)
            {
                const vbo::layout::attribute &tc=v.layout.tc[i];
                if(tc.dimension>0)
                {
                    //if(vobj.vertex_stride!=active_attributes.vertex_stride || !tc.compare(active_attributes.tcs[i]))
                    {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                        if(!applied_layout.tc[i].dimension)
                            glEnableVertexAttribArray(tc0_attribute+i);
                        glVertexAttribPointer(tc0_attribute+i,tc.dimension,get_gl_element_type(tc.type),gl_attrib_normalized(tc.type),
                                              v.stride,(void*)(ptrdiff_t)(tc.offset));
#else
                        glClientActiveTexture(GL_TEXTURE0_ARB+i);
                        if(!applied_layout.tc[i].dimension)
                            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                        glTexCoordPointer(tc.dimension,get_gl_element_type(tc.type),v.stride,(void*)(ptrdiff_t)(tc.offset));
#endif
                    }
                }
                else if(applied_layout.tc[i].dimension>0)
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    glDisableVertexAttribArray(tc0_attribute+i);
#else
                    glClientActiveTexture(GL_TEXTURE0_ARB+i);
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
                }
            }

            if(v.layout.normal.dimension>0)
            {
                //if(vobj.vertex_stride!=active_attributes.vertex_stride || !vobj.normals.compare(active_attributes.normals))
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    if(!applied_layout.normal.dimension)
                        glEnableVertexAttribArray(normal_attribute);
                    glVertexAttribPointer(normal_attribute,3,get_gl_element_type(v.layout.normal.type),gl_attrib_normalized(v.layout.normal.type),
                                          v.stride,(void*)(ptrdiff_t)(v.layout.normal.offset));
#else
                    if(!applied_layout.normal.dimension)
                        glEnableClientState(GL_NORMAL_ARRAY);
                    glNormalPointer(get_gl_element_type(v.layout.normal.type),v.stride,(void*)(ptrdiff_t)(v.layout.normal.offset));
#endif
                }
            }
            else if(applied_layout.normal.dimension>0)
            {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                glDisableVertexAttribArray(normal_attribute);
#else
                glDisableClientState(GL_NORMAL_ARRAY);
#endif
            }

            if(v.layout.color.dimension>0)
            {
                //if(vobj.vertex_stride!=active_attributes.vertex_stride || !vobj.colors.compare(active_attributes.colors))
                {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                    if(!applied_layout.color.dimension)
                        glEnableVertexAttribArray(color_attribute);
                    glVertexAttribPointer(color_attribute,v.layout.color.dimension,get_gl_element_type(v.layout.color.type),gl_attrib_normalized(v.layout.color.type),
                                          v.stride,(void*)(ptrdiff_t)(v.layout.color.offset));
#else
                    if(!applied_layout.color.dimension)
                        glEnableClientState(GL_COLOR_ARRAY);
                    glColorPointer(v.layout.color.dimension,get_gl_element_type(v.layout.color.type),
                                   v.stride,(void*)(ptrdiff_t)(v.layout.color.offset));
#endif
                }
            }
            else if(applied_layout.color.dimension>0)
            {
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
                glDisableVertexAttribArray(color_attribute);
#else
                glDisableClientState(GL_COLOR_ARRAY);
#endif
            }
            applied_layout=v.layout;
#ifdef USE_VAO
        }
#endif
        applied_state.vertex_buffer=s.vertex_buffer;
    }

    const int gl_elem=get_gl_element_type(s.primitive);
    //TEMP DEBUG (iOS): draw breadcrumbs for simulator crash diagnosis
    static int oh_trace_draws=-1;
    if(oh_trace_draws<0)
        oh_trace_draws=getenv("OH_TRACE_DRAW")?atoi(getenv("OH_TRACE_DRAW")):0;
    if(oh_trace_draws>0 && oh_trace_draws<200000)
    {
        fprintf(stderr,"OH_DRAW vbo=%d(pos=%d,n=%d,c=%d,tc=",s.vertex_buffer,
                v.layout.pos.dimension,v.layout.normal.dimension,v.layout.color.dimension);
        for(unsigned int t=0;t<vbo::max_tex_coord;++t) fprintf(stderr,"%d",v.layout.tc[t].dimension);
        fprintf(stderr,",stride=%d) ib=%d off=%u cnt=%u inst=%u shd=%d prim=%d #%d\n",
                v.stride,s.index_buffer,s.index_offset,s.index_count,s.instances_count,
                s.shader,s.primitive,oh_trace_draws);
        //TEMP DEBUG: full read-only attribute/element state near the crash window
        if(oh_trace_draws>10800)
        {
            GLint vao=0,eb=0,ab=0;
            glGetIntegerv(0x85B5 /*GL_VERTEX_ARRAY_BINDING*/,&vao);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&eb);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&ab);
            fprintf(stderr,"  bind vao=%u elem=%u array=%u",vao,eb,ab);
            for(int a=0;a<8;++a)
            {
                GLint en=0,buf=0;void *ptr=0;GLint sz=0,tp=0,nrm=0,st=0;
                glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&en);
                if(!en) { fprintf(stderr," a%d:off",a); continue; }
                glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buf);
                glGetVertexAttribPointerv(a,GL_VERTEX_ATTRIB_ARRAY_POINTER,&ptr);
                glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_SIZE,&sz);
                fprintf(stderr," a%d:%u/%x/%p",a,buf,sz,ptr);
            }
            fprintf(stderr,"\n");
        }
        ++oh_trace_draws;
    }
    if(s.index_buffer>=0)
    {
        ind_buf &i=ind_bufs.get(s.index_buffer);

#ifdef USE_VAO
        if(i.id!=v.active_vao_ibuf)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i.id);
            v.active_vao_ibuf=i.id;
        }
#else
        if(s.index_buffer!=applied_state.index_buffer)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i.id);
            applied_state.index_buffer=s.index_buffer;
        }
#endif
        const unsigned int gl_elem_type=(i.type==vbo::index4b?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT);
#ifdef USE_INSTANCING
        if(s.instances_count>1 && glDrawElementsInstancedARB)
            glDrawElementsInstancedARB(gl_elem,s.index_count,gl_elem_type,(void*)(ptrdiff_t)(s.index_offset*i.type),s.instances_count);
        else
#endif
            glDrawElements(gl_elem,s.index_count,gl_elem_type,(void*)(ptrdiff_t)(s.index_offset*i.type));
    }
    else
    {
#ifdef USE_INSTANCING
        if(s.instances_count>1 && glDrawArraysInstancedARB)
            glDrawArraysInstancedARB(gl_elem,s.index_offset,s.index_count,s.instances_count);
        else
#endif
            glDrawArrays(gl_elem,s.index_offset,s.index_count);
    }
}

void render_opengl::log_errors(const char *place)
{
#ifdef OPENGL
    for(int i=glGetError();i!=GL_NO_ERROR;i=glGetError())
    {
        log()<<"gl error: ";
        switch(i)
        {
            case GL_INVALID_ENUM: log()<<"invalid enum"; break;
            case GL_INVALID_VALUE: log()<<"invalid value"; break;
            case GL_INVALID_OPERATION: log()<<"invalid operation"; break;
#if !defined OPENGL_ES && !defined OPENGL3
            case GL_STACK_OVERFLOW: log()<<"stack overflow"; break;
            case GL_STACK_UNDERFLOW: log()<<"stack underflow"; break;
#endif
            case GL_OUT_OF_MEMORY: log()<<"out of memory"; break;

            default: log()<<"unknown"; break;
        }

        log()<<" ("<<i<<")";
        if(place)
            log()<<" at "<<place<<"\n";
        else
            log()<<"\n";
    }
#endif
}

unsigned int render_opengl::get_gl_texture_id(int idx)
{
    return textures.get(idx).tex_id;
}

void render_opengl::gl_bind_texture2d(uint gl_tex,uint layer)
{
    gl_bind_texture(GL_TEXTURE_2D,gl_tex,layer);
}

void render_opengl::gl_bind_texture(uint gl_type,uint gl_tex,uint layer)
{
    set_texture(0,layer);
    gl_select_multitex_layer((int)layer);
    glBindTexture(gl_type, gl_tex);
}

render_opengl &render_opengl::get() { static render_opengl *api = new render_opengl(); return *api; }
}
