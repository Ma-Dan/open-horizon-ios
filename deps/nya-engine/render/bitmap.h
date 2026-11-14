//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

namespace nya_render
{

void bitmap_downsample2x(unsigned char *data,int width,int height,int channels);
void bitmap_downsample2x(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_flip_vertical(unsigned char *data,int width,int height,int channels);
void bitmap_flip_vertical(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_flip_horisontal(unsigned char *data,int width,int height,int channels);
void bitmap_flip_horisontal(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_rotate_90_left(unsigned char *data,int width,int height,int channels);
void bitmap_rotate_90_left(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_rotate_90_right(unsigned char *data,int width,int height,int channels);
void bitmap_rotate_90_right(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_rotate_180(unsigned char *data,int width,int height,int channels);
void bitmap_rotate_180(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_crop(unsigned char *data,int width,int height,int x,int y,int crop_width,int crop_height,int channels);
void bitmap_crop(const unsigned char *data,int width,int height,int x,int y,int crop_width,int crop_height,int channels,unsigned char *out);

void bitmap_resize(const unsigned char *data,int width,int height,int new_width,int new_height,int channels,unsigned char *out);

void bitmap_rgb_to_bgr(unsigned char *data,int width,int height,int channels);
void bitmap_rgb_to_bgr(const unsigned char *data,int width,int height,int channels,unsigned char *out);

void bitmap_rgba_to_rgb(unsigned char *data,int width,int height);
void bitmap_rgba_to_rgb(const unsigned char *data,int width,int height,unsigned char *out);
void bitmap_rgb_to_rgba(const unsigned char *data,int width,int height,unsigned char alpha,unsigned char *out);

void bitmap_rgb_to_yuv420(const unsigned char *data,int width,int height,int channels,unsigned char *out);
void bitmap_bgr_to_yuv420(const unsigned char *data,int width,int height,int channels,unsigned char *out);
void bitmap_yuv420_to_rgb(const unsigned char *data,int width,int height,unsigned char *out);

}
