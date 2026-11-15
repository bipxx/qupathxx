#pragma once
#include "gui_i18n.hpp"
#include <cstdio>
#include <cstring>

namespace qpx::ui {
// Parts are appended atomically: a full buffer never leaves half a separator.
template<std::size_t N> inline void status_part(char (&out)[N],const char* part) {
    if(!part || !*part)return;const auto used=std::strlen(out),length=std::strlen(part);const std::size_t gap=used?3:0;
    if(used+gap+length>=N)return;
    if(gap)std::memcpy(out+used," | ",gap);std::memcpy(out+used+gap,part,length+1);
}
template<std::size_t N> inline void status_summary(char (&out)[N],bool image,bool project,bool visible,bool dirty,
        std::size_t matched,std::size_t images,long long width,long long height,double zoom,std::size_t objects,std::size_t selected) {
    out[0]='\0';char part[192]{};
    if(!visible)status_part(out,i18n::text("Objects hidden"));
    if(project){std::snprintf(part,sizeof(part),i18n::text("%zu of %zu images"),matched,images);status_part(out,part);}
    if(!image){status_part(out,i18n::text("Ready  |  Open a project or add an image to begin"));return;}
    std::snprintf(part,sizeof(part),"%lld x %lld px",width,height);status_part(out,part);
    std::snprintf(part,sizeof(part),"%s %.0f%%",i18n::text("Zoom"),zoom);status_part(out,part);
    std::snprintf(part,sizeof(part),"%zu %s",objects,i18n::text("objects"));status_part(out,part);
    std::snprintf(part,sizeof(part),"%zu %s",selected,i18n::text("selected"));status_part(out,part);
    if(project && dirty)status_part(out,i18n::text("Unsaved project"));
}
}
