namespace std { using uint64_t=__UINT64_TYPE__; using size_t=__SIZE_TYPE__; }
struct Axis {int samples() const;};
struct Grid {Axis x() const; Axis y() const;};
struct Classes {std::size_t size() const;};
struct Forest {Classes classes() const;};
struct Model {Forest forest() const;};
struct Data {Grid grid;Model model;};
struct Cursor {std::size_t p;std::uint64_t integer();};
void dimensions(const Data& d){const auto count=std::uint64_t(d.grid.x().samples())*std::uint64_t(d.grid.y().samples());const auto classes=d.model.forest().classes().size();}
void position(Cursor& c){const auto at=c.p;const auto id=c.integer();}
