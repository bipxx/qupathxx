#include <algorithm>
#include <cstddef>
#include <type_traits>
static_assert(sizeof(std::size_t)==sizeof(unsigned long long));
static_assert(!std::is_same_v<std::size_t,unsigned long long>);
struct Grid {long long width() const {return 3;}long long height() const {return 3;}};
struct Decoder {unsigned long long integer(unsigned long long) {return 1;}};
void check() { Grid grid;Decoder d;struct {std::size_t max_cells=10;} p;
const auto n=std::size_t(grid.width()*grid.height()),samples=d.integer(n),threshold=d.integer(samples),raw=d.integer(std::min(p.max_cells,n)),markers=d.integer(raw),count=d.integer(std::min(markers,samples));
(void)n;(void)samples;(void)threshold;(void)raw;(void)markers;(void)count;
}
