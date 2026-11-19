// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/ome_zarr.hpp"
#include "reader_plugin.hpp"
#include <iostream>
#include <stdexcept>
int main() {
  try {
    if (qpx::has_ome_zarr())
      throw std::runtime_error("Expected the decoder-OFF route");
    try {
      qpx::detail::open_bounded_plugin_image_server("unused", {}, 65536);
    } catch (const std::runtime_error &e) {
      if (std::string(e.what()).find("QUPATHXX_BUILD_READER_PLUGINS=ON") ==
          std::string::npos)
        throw;
      std::cout << "Bounded plugin-open OFF symbol and explicit disabled-route "
                   "refusal passed\n";
      return 0;
    }
    throw std::runtime_error("Disabled bounded plugin open succeeded");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
