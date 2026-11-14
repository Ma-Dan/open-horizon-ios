//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "bitmap.h"
#include "memory/tmp_buffer.h"
#include <string.h>
#include <vector>

namespace nya_render
{

void bitmap_downsample_x(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    for(int h=0;h<height;++h)
    {
        const unsigned char *hdata=data+(h*channels)*width;
        for(int w=0;w<width/2;++w,hdata+=channels*2)
        {
            const unsigned char *wdata=hdata;
            for(int c=0;c<channels;++c,++wdata)
                *out++ =(wdata[0]+wdata[channels])/2;
        }
    }
}

void bitmap_downsample_y(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    const int stride=channels*width;
    for(int h=0;h<height/2;++h)
    {
        const unsigned char *hdata=data+h*stride*2;
        for(int w=0;w<width;++w,hdata+=channels)
        {
            const unsigned char *wdata=hdata;
            for(int c=0;c<channels;++c,++wdata)
                *out++ =(wdata[0] + wdata[stride])/2;
        }
    }
}

void bitmap_downsample2x(unsigned char *data,int width,int height,int channels)
{
    if(width>1)
    {
        bitmap_downsample_x(data,width,height,channels,data);
        width/=2;
    }
    if(height>1)
        bitmap_downsample_y(data,width,height,channels,data);
}

void bitmap_downsample2x(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    if(width<=1)
    {
        if(height>1)
            bitmap_downsample_y(data,width,height,channels,out);
        return;
    }

    if(height<=1)
    {
        if(width>1)
            bitmap_downsample_x(data,width,height,channels,out);
        return;
    }

    for(int h=0;h<height/2;++h)
    {
        const unsigned char *hdata=data+(h*channels*2)*width;
        for(int w=0;w<width/2;++w,hdata+=channels*2)
        {
            const unsigned char *wdata=hdata;
            for(int c=0;c<channels;++c,++wdata)
            {
                *out++ =(wdata[0]+
                         wdata[channels]+
                         wdata[channels*width]+
                         wdata[channels*width+channels])/4;
            }
        }
    }
}

inline void swap(unsigned char *a,unsigned char *b,int channels)
{
    unsigned char tmp;
    for(int i=0;i<channels;++i)
        tmp=a[i],a[i]=b[i],b[i]=tmp;
}

void bitmap_flip_vertical(unsigned char *data,int width,int height,int channels)
{
    const int line_size=width*channels;
    const int half=line_size*(height/2);
    const int top=line_size*(height-1);
    std::vector<unsigned char> line_data(line_size);
    unsigned char *line=&line_data[0], *from=data, *to=data+top;

    for(int offset=0;offset<half;offset+=line_size)
    {
        unsigned char *f=from+offset, *t=to-offset;
        memcpy(line,f,line_size);
        memcpy(f,t,line_size);
        memcpy(t,line,line_size);
    }
}

void bitmap_flip_vertical(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    const int line_size=width*channels;
    out+=line_size*(height-1);
    for(int y=0;y<height;++y,data+=line_size,out-=line_size)
        memcpy(out,data,line_size);
}

void bitmap_flip_horisontal(unsigned char *data,int width,int height,int channels)
{
    const int line_size=width*channels;
    const int size=width*height*channels;
    const int half=line_size/2;

    for(int offset=0;offset<size;offset+=line_size)
    {
        unsigned char *ha=data+offset;
        unsigned char *hb=ha+line_size-channels;

        for(int w=0;w<half;w+=channels)
            swap(ha+w,hb-w,channels);
    }
}

void bitmap_flip_horisontal(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    const int line_size=width*channels;
    out+=(width-1)*channels;
    for(int y=0;y<height;++y,data+=line_size,out+=line_size)
    {
        for(int w=0;w<line_size;w+=channels)
            memcpy(out-w,data+w,channels);
    }
}

void bitmap_rotate_90_left(unsigned char *data,int width,int height,int channels)
{
    if(width!=height)
    {
        nya_memory::tmp_buffer_scoped buf(width*height*channels);
        bitmap_rotate_90_left(data,width,height,channels,(unsigned char *)buf.get_data());
        buf.copy_to(data,buf.get_size());
        return;
    }

    for(int y=0;y<width;++y)
    {
        for(int x=y+1;x<width;++x)
            swap(data+(y*width+x)*channels,data+(x*width+y)*channels,channels);
    }

    bitmap_flip_horisontal(data,width,height,channels);
}

void bitmap_rotate_90_left(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    for(int x=0;x<height;++x)
    {
        for(int y=0;y<width;++y,data+=channels)
            memcpy(out+(y*height+height-x-1)*channels,data,channels);
    }
}

void bitmap_rotate_90_right(unsigned char *data,int width,int height,int channels)
{
    if(width!=height)
    {
        nya_memory::tmp_buffer_scoped buf(width*height*channels);
        bitmap_rotate_90_right(data,width,height,channels,(unsigned char *)buf.get_data());
        buf.copy_to(data,buf.get_size());
        return;
    }

    for(int y=0;y<width;++y)
    {
        for(int x=y+1;x<width;++x)
            swap(data+(y*width+x)*channels,data+(x*width+y)*channels,channels);
    }

    bitmap_flip_vertical(data,width,height,channels);
}

void bitmap_rotate_90_right(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    for(int x=0;x<height;++x)
    {
        for(int y=0;y<width;++y,data+=channels)
            memcpy(out+((width-y-1)*height+x)*channels,data,channels);
    }
}

void bitmap_rotate_180(unsigned char *data,int width,int height,int channels)
{
    const int line_size=width*channels;
    const int size=width*height*channels;
    const int top=line_size*(height-1);
    const int half=line_size/2;

    for(int offset=0;offset<size;offset+=line_size)
    {
        unsigned char *ha=data+top-offset;
        unsigned char *hb=data+offset+line_size-channels;

        for(int w=0;w<half;w+=channels)
            swap(ha+w,hb-w,channels);
    }

    if((width%2)==1)
    {
        for(int y=0;y<height/2;++y)
            swap(data+(width*y+width/2)*channels,data+(width*(height-y-1)+width/2)*channels,channels);
    }
}

void bitmap_rotate_180(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    out+=(height-1)*width*channels+(width-1)*channels;
    for(int i=0;i<width*height;++i,out-=channels,data+=channels)
        memcpy(out,data,channels);
}

void bitmap_crop(unsigned char *data,int width,int height,int x,int y,int crop_width,int crop_height,int channels)
{
    bitmap_crop(data,width,height,x,y,crop_width,crop_height,channels,data);
}

void bitmap_crop(const unsigned char *data,int width,int height,int x,int y,int crop_width,int crop_height,int channels,unsigned char *out)
{
    if(x<0)
        x=0;
    if(y<0)
        y=0;
    if(x+crop_width>width)
        crop_width=width-x;
    if(y+crop_height>height)
        crop_height=height-y;
    if(crop_width<=0 || crop_height<=0)
        return;

    data+=(width*y+x)*channels;
    const int line_size=width*channels;
    const int crop_line_size=crop_width*channels;
    for(int i=0;i<crop_height;++i,out+=crop_line_size,data+=line_size)
        memmove(out,data,crop_line_size);
}

void bitmap_resize(const unsigned char *data,int width,int height,int new_width,int new_height,int channels,unsigned char *out)
{
    const unsigned int stride=width*channels;
    const unsigned int stride_1=(width+1)*channels;
    const unsigned int x_ratio=((width-1)<<16)/new_width;
    const unsigned int y_ratio=((height-1)<<16)/new_height;
    unsigned long int y=0;
    for(int i=0;i<new_height;++i)
    {
        const unsigned int yr=(unsigned int)(y>>16),y_index=yr*width;
        const unsigned long int dy=y-(yr<<16),dy2=(1<<16)-dy;
        unsigned long int x=0;
        for(int j=0;j<new_width;++j)
        {
            const unsigned int xr=(unsigned int)(x>>16);
            const unsigned long int dx=x-(xr<<16),dx2=(1<<16)-dx;
            const unsigned char *cdata=data+(y_index+xr)*channels;
            for(int k=0;k<channels;++k,++cdata)
            {
                *out++ = (unsigned char)((cdata[0]*dx2*dy2+
                                          cdata[channels]*dx*dy2+
                                          cdata[stride]*dy*dx2+
                                          cdata[stride_1]*dx*dy)>>32);
            }
            x+=x_ratio;
        }
        y+=y_ratio;
    }
}

void bitmap_rgb_to_bgr(unsigned char *data,int width,int height,int channels)
{
    if(channels<3)
        return;

    for(int i=0;i<width*height;++i,data+=channels)
    {
        const unsigned char tmp=data[0];
        data[0]=data[2];
        data[2]=tmp;
    }
}

void bitmap_rgb_to_bgr(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    if(channels==3)
    {
        for(int i=0;i<width*height;++i,data+=3,out+=3)
            out[0]=data[2],out[1]=data[1],out[2]=data[0];
    }
    else if(channels==4)
    {
        for(int i=0;i<width*height;++i,data+=4,out+=4)
            out[0]=data[2],out[1]=data[1],out[2]=data[0],out[3]=data[3];
    }
    else
        memcpy(out,data,width*height*channels);
}

void bitmap_rgba_to_rgb(unsigned char *data,int width,int height)
{
    bitmap_rgba_to_rgb(data,width,height,data);
}

void bitmap_rgba_to_rgb(const unsigned char *data,int width,int height,unsigned char *out)
{
    for(int i=0;i<width*height;++i,data+=4,out+=3)
    {
        out[0]=data[0];
        out[1]=data[1];
        out[2]=data[2];
    }
}

void bitmap_rgb_to_rgba(const unsigned char *data,int width,int height,unsigned char alpha,unsigned char *out)
{
    data+=width*height*3;
    out+=width*height*4;
    for(int i=0;i<width*height;++i)
    {
        out-=4,data-=3;
        out[0]=data[0];
        out[1]=data[1];
        out[2]=data[2];
        out[3]=alpha;
    }
}

inline void color_to_yuv420(const unsigned char *data[3],int width,int height,int channels,unsigned char *out)
{
    int image_size=width*height;
    unsigned char *dst_y=out;
    unsigned char *dst_u=dst_y+image_size;
    unsigned char *dst_v=dst_u+image_size/4;
    image_size*=channels;

    for(int i=0;i<image_size;i+=channels)
        *dst_y++ =((66*data[0][i] + 129*data[1][i] + 25*data[2][i])>>8)+16;

    const int stride=width*channels;
    const int channels2=channels*2;
    for(int y=0,i=0;y<height;y+=2,i+=stride)
    {
        for(int x=0;x<width;x+=2,i+=channels2)
        {
            *dst_u++ =((-38*data[0][i] - 74*data[1][i] + 112*data[2][i])>>8)+128;
            *dst_v++ =((112*data[0][i] - 94*data[1][i] -  18*data[2][i])>>8)+128;
        }
    }
}

void bitmap_rgb_to_yuv420(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    const unsigned char *rgb[]={data,data+1,data+2};
    if(channels==1)
        rgb[2]=rgb[1]=rgb[0];
    color_to_yuv420(rgb,width,height,channels,out);
}

void bitmap_bgr_to_yuv420(const unsigned char *data,int width,int height,int channels,unsigned char *out)
{
    const unsigned char *bgr[]={data+2,data+1,data};
    if(channels==1)
        bgr[0]=bgr[1]=bgr[2];
    color_to_yuv420(bgr,width,height,channels,out);
}

inline unsigned char clamp(int c) { return c<0?0:(c>255?255:c); }

void bitmap_yuv420_to_rgb(const unsigned char *data,int width,int height,unsigned char *out)
{
    const size_t image_size=width*height;
    const unsigned char *udata=data+image_size;
    const unsigned char *vdata=udata+image_size/4;
    const int half_width=width/2;

    for(int y=0;y<height;++y)
    {
        const int hidx=y/2*half_width;
        for(int x=0;x<width;++x)
        {
            const int y0=(int)*data++;
            const int idx=hidx+x/2;
            const int u0=(int)udata[idx]-128;
            const int v0=(int)vdata[idx]-128;

            const int y1=1192*(y0>16?y0-16:0);
            const int u1=400*u0, u2=2066*u0;
            const int v1=1634*v0, v2=832*v0;

            *out++ =clamp((y1+v1)>>10);
            *out++ =clamp((y1-v2-u1)>>10);
            *out++ =clamp((y1+u2)>>10);
        }
    }
}

}
