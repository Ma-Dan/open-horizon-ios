//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "render_api.h"

namespace nya_render
{

class render_directx11: public render_api_interface
{
public:
    bool is_available() const override;

public:
    void clear(const viewport_state &s,bool color,bool depth,bool stencil) override;
    void draw(const state &s) override {}

public:
    void invalidate_cached_state() override;
    void apply_state(const state &s) override;

public:
    static render_directx11 &get();

private:
    render_directx11() {}
};

}
