#include "qupathxx/display.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qpx {
void DisplaySettings::validate() const {
    if (!std::isfinite(black) || !std::isfinite(white) || !std::isfinite(gamma) ||
        black<0 || white>255 || white<=black || gamma<.05 || gamma>20 ||
        mode<DisplayMode::RGB || mode>DisplayMode::Blue)
        throw std::invalid_argument("display requires 0 <= black < white <= 255 and 0.05 <= gamma <= 20");
}
bool DisplaySettings::operator==(const DisplaySettings& b) const { return mode==b.mode && black==b.black && white==b.white && gamma==b.gamma; }
std::array<std::uint8_t,256> display_lut(const DisplaySettings& s) {
    s.validate(); std::array<std::uint8_t,256> result;
    for (int i=0;i<256;++i) result[i]=static_cast<std::uint8_t>(std::lround(255*std::pow(std::clamp((i-s.black)/(s.white-s.black),0.,1.),1/s.gamma)));
    return result;
}
std::vector<std::uint8_t> display_pixels(const PixelBuffer& pixels, const DisplaySettings& settings) {
    auto lut=display_lut(settings); auto result=pixels.bytes();
    if (settings==DisplaySettings{}) return result;
    for (std::size_t i=0;i<result.size();i+=4) {
        if (settings.mode==DisplayMode::RGB) for (int c=0;c<3;++c) result[i+c]=lut[result[i+c]];
        else {
            unsigned v=0;
            if (settings.mode==DisplayMode::Gray) v=(77u*result[i]+150u*result[i+1]+29u*result[i+2]+128u)>>8;
            else v=result[i+static_cast<unsigned>(settings.mode)-static_cast<unsigned>(DisplayMode::Red)];
            result[i]=result[i+1]=result[i+2]=lut[v];
        }
        if (!result[i+3]) result[i]=result[i+1]=result[i+2]=0;
    }
    return result;
}
}
