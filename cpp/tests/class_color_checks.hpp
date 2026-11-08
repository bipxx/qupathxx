#pragma once
#include "../qupath_class_color.hpp"
#include <iostream>
namespace class_color_checks {
inline void run() {
    struct Row {std::vector<std::string> path;std::optional<std::uint32_t> parent;std::uint32_t expected;};
    const std::vector<Row> rows={
        {{u8"Positive"},std::nullopt,0xfffa3e3eu},
        {{u8"Negative"},std::nullopt,0xff7070e1u},
        {{u8"1+"},std::nullopt,0xffffff00u},
        {{u8"2+"},std::nullopt,0xffffbb3eu},
        {{u8"3+"},std::nullopt,0xfffa3e3eu},
        {{u8"Unseen \u03b1"},std::nullopt,0xff3352a3u},
        {{u8"Class \U0001f680"},std::nullopt,0xff65d0adu},
        {{u8"\u00e9"},std::nullopt,0xffbba457u},
        {{u8"e\u0301"},std::nullopt,0xffccbfc7u},
        {{u8"Dark candidate 141"},std::nullopt,0xffcc8d61u},
        {{u8"Tumor",u8"1+"},0x00c80000u,0xffffd700u},
        {{u8"Tumor",u8"2+"},0x00c80000u,0xffe19632u},
        {{u8"Tumor",u8"3+"},0x00c80000u,0xffc83232u},
        {{u8"Tumor",u8"Positive"},0x00c80000u,0xffc83232u},
        {{u8"Tumor",u8"Negative"},0x00c80000u,0xff5a5ab4u},
        {{u8"Tumor",u8"Unseen child"},0x00c80000u,0xffe101f5u},
        {{u8"Tumor",u8"Child \U0001f680"},0x00c80000u,0xffdd7585u},
        {{u8"Other base",u8"1+"},0x00204080u,0xff1c3973u},
        {{u8"Other base",u8"2+"},0x00204080u,0xff13264cu},
        {{u8"Other base",u8"3+"},0x00204080u,0xff0c1933u},
        {{u8"Other base",u8"Positive"},0x00204080u,0xff183060u},
        {{u8"Other base",u8"Negative"},0x00204080u,0xff2850a0u},
        {{u8"Other base",u8"Unseen child"},0x00204080u,0xff1b9b70u},
        {{u8"Other base",u8"Child \U0001f680"},0x00204080u,0xff5c076cu},
        {{u8"Other base",u8"Branch",u8"1+"},0x00102030u,0xff0e1c2bu},
        {{u8"Other base",u8"Branch",u8"2+"},0x00102030u,0xff09131cu},
        {{u8"Other base",u8"Branch",u8"3+"},0x00102030u,0xff060c13u},
        {{u8"Other base",u8"Branch",u8"Positive"},0x00102030u,0xff0c1824u},
        {{u8"Other base",u8"Branch",u8"Negative"},0x00102030u,0xff14283cu},
        {{u8"Other base",u8"Branch",u8"Unseen child"},0x00102030u,0xff57d404u},
        {{u8"Other base",u8"Branch",u8"Child \U0001f680"},0x00102030u,0xff7668fcu},
        {{u8"Alpha base",u8"1+"},0x80204080u,0xff1c3973u},
        {{u8"Alpha base",u8"2+"},0x80204080u,0xff13264cu},
        {{u8"Alpha base",u8"3+"},0x80204080u,0xff0c1933u},
        {{u8"Alpha base",u8"Positive"},0x80204080u,0xff183060u},
        {{u8"Alpha base",u8"Negative"},0x80204080u,0xff2850a0u},
        {{u8"Alpha base",u8"Unseen child"},0x80204080u,0xffe96b95u},
        {{u8"Alpha base",u8"Child \U0001f680"},0x80204080u,0xff733a05u},
        {{u8"Cleared color"},std::nullopt,0xffc81767u},
    };
    for(const auto& row:rows)if(qpx::detail::qupath_class_default_argb(row.path,row.parent)!=row.expected)throw std::runtime_error("default class color differs from independent QuPath oracle");
    for(const auto& path:std::vector<std::vector<std::string>>{{},{""},{std::string(4097,'x')},{std::string(1,char(0xff))},std::vector<std::string>(33,"x")}) {
        bool refused=false;try{qpx::detail::qupath_class_default_argb(path);}catch(const std::invalid_argument&){refused=true;}
        if(!refused)throw std::runtime_error("invalid default-color path admitted");
    }
    bool refused=false;try{qpx::detail::qupath_class_default_argb({"Parent","Child"});}catch(const std::invalid_argument&){refused=true;}
    if(!refused)throw std::runtime_error("unresolved parent default color admitted");
    std::cout<<"Independent QuPath default colors:39 cases, UTF16/normalization, parent/ARGB/scaling and dark rejection passed\n";
}
}
