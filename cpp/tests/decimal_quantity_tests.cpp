#include "../decimal_quantity.hpp"
#include <atomic>
#include <clocale>
#include <iostream>
#include <locale>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value) {
    if (!value) throw std::runtime_error("decimal quantity contract failed");
}
struct Comma : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
void check() {
    using qpx::detail::positive_decimal_quantity;
    for (auto text : {"1.25", "001.25", "1.25e0", "125e-2", ".125e+1", "1.25000"})
        require(positive_decimal_quantity(text) == 1.25);
    require(positive_decimal_quantity("1.") == 1.);
    require(positive_decimal_quantity("1e308").has_value());
    require(positive_decimal_quantity("1e-308").value_or(0) > 0);
    require(positive_decimal_quantity("4.9406564584124654e-324").value_or(0) > 0);
    for (auto text : {"", ".", "0", "-1", "+1", " 1", "\t1", "1 ", "1\n", "1,25", "1px",
                      "1e", "1e+", "nan", "inf", "Infinity", "0x1", "0X1p0", "1e309", "1e-9999"})
        require(!positive_decimal_quantity(text));
    require(!positive_decimal_quantity(nullptr));
}
}
int main() {
    try {
        // Concurrent first use exercises native static locale initialization.
        std::atomic<int> ready{0}; std::atomic<bool> go{false}, failed{false};
        std::vector<std::thread> threads;
        for (int i=0;i<8;++i) threads.emplace_back([&] {
            ++ready; while (!go.load()) std::this_thread::yield();
            try { for (int j=0;j<100;++j) check(); } catch (...) { failed=true; }
        });
        while (ready.load()!=8) std::this_thread::yield(); go=true;
        for (auto& thread:threads) thread.join();
        require(!failed);
        const std::string before=std::setlocale(LC_NUMERIC,nullptr);
        const auto previous=std::locale::global(std::locale(std::locale::classic(),new Comma));
        check();
        require(std::use_facet<std::numpunct<char>>(std::locale()).decimal_point()==',');
        require(before==std::setlocale(LC_NUMERIC,nullptr));
        std::locale::global(previous);
        bool comma_locale=false;
        for (auto name:{"de_DE.UTF-8","de_DE.utf8","de_DE","German_Germany.1252"}) {
            if (!std::setlocale(LC_NUMERIC,name)) continue;
            const std::string selected=std::setlocale(LC_NUMERIC,nullptr);
            if (std::string(std::localeconv()->decimal_point)==",") {
                check(); require(selected==std::setlocale(LC_NUMERIC,nullptr));
                comma_locale=true; break;
            }
        }
        require(std::setlocale(LC_NUMERIC,before.c_str())!=nullptr);
        std::cout << "installed comma C locale exercised: " << comma_locale << '\n';
        std::cout << "decimal quantities: strict syntax, range, concurrency and locale checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
