#include "../gui_overlay.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool ok) {if(!ok) throw std::runtime_error("overlay surface check failed");}
int main() {
    try {
        for(float scale:{1.f,1.25f,1.5f,2.f}) {
            const auto s=overlay_surface({10.3f,20.7f,99.2f,88.1f},{200,120},{scale,scale},4096,1024*1024);
            check(bool(s));
            check(s->x==int(std::floor(10.3*scale)) && s->y==int(120*scale)-int(std::ceil(88.1*scale)));
            check(s->top_left.x<=10.3f && s->top_left.y<=20.7f && s->bottom_right.x>=99.2f && s->bottom_right.y>=88.1f);
            check(s->bytes()==std::size_t(s->width)*s->height*4);
            check(overlay_surface({10.3f,20.7f,99.2f,88.1f},{200,120},{scale,scale},4096,s->bytes()).has_value());
            check(!overlay_surface({10.3f,20.7f,99.2f,88.1f},{200,120},{scale,scale},4096,s->bytes()-1));
        }
        auto clipped=overlay_surface({-20,-30,1000,1000},{200,120},{1,1},4096,1000000);
        check(clipped && clipped->x==0 && clipped->y==0 && clipped->width==200 && clipped->height==120);
        check(!overlay_surface({0,0,200,120},{200,120},{1,1},100,1000000));
        check(!overlay_surface({0,0,200,120},{200,120},{0,1},4096,1000000));
        check(!overlay_surface({0,0,200,120},{0,0},{1,1},4096,1000000));
        check(!overlay_surface({10,20,5,4},{200,120},{1,1},4096,1000000));
        check(!overlay_surface({0,0,200,120},{200,120},{1,1},4096,0));
        auto fractional=overlay_surface({0,0,640,480},{640,480},{1.3f,1.3f},4096,10000000);
        check(fractional && fractional->frame_width==int(640.f*1.3f) && fractional->frame_height==int(480.f*1.3f));
        std::cout<<"Physical canvas bounds, DPI alignment and exact budget endpoints passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
