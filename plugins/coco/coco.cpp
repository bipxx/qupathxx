// SPDX-License-Identifier: AGPL-3.0-only
#include "coco_mask.hpp"
#include "format_plugin_profile.hpp"
#include <unordered_map>
namespace {
using namespace qpx_coco;
using Index = std::pmr::unordered_map<std::uint64_t, std::size_t>;
struct Definition {
  static constexpr const char *id = "org.qupathxx.coco";
  static constexpr const char *format = "coco";
  static constexpr const char *name = "COCO instance annotations";
  static constexpr const char *contract = "coco-instance-pixel-masks/1";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1 v) {
    require(v.data && v.size,
            "COCO requires explicit image_id provider options");
  }
  static void decode(Handle &h, Bytes input,
                     const qpx_reader_open_v1 &request) {
    auto *memory = &h.arena.memory;
    Tree options({reinterpret_cast<const char *>(request.options.data),
                  std::size_t(request.options.size)},
                 memory, request.cancel);
    options.keys(options.root(), {"image_id"});
    const auto selected_id = integer(options.get(options.root(), "image_id"));
    Tree tree(input.view(), memory, request.cancel);
    tree.keys(tree.root(),
              {"info", "licenses", "images", "annotations", "categories"});
    const auto &images = tree.array(tree.get(tree.root(), "images"));
    const auto &categories = tree.array(tree.get(tree.root(), "categories"));
    const auto &annotations = tree.array(tree.get(tree.root(), "annotations"));
    valid(!images.empty() && images.size() <= 100000 &&
              categories.size() <= 100000 && annotations.size() <= 100000,
          "COCO catalogue dimensions exceed native bounds");
    Index image_ids(memory), category_ids(memory), annotation_ids(memory);
    auto index = [&](const auto &array, Index &out) {
      for (const auto i : array) {
        poll(request.cancel);
        const auto key = integer(tree.get(tree.nodes[i], "id"));
        valid(out.emplace(key, i).second, "Duplicate COCO catalogue ID");
      }
    };
    index(images, image_ids);
    index(categories, category_ids);
    index(annotations, annotation_ids);
    valid(image_ids.count(selected_id) == 1,
          "Selected COCO image ID is absent");
    auto dimension = [&](const Node &image, std::string_view axis) {
      const auto n = integer(tree.get(image, axis));
      valid(n && n <= 1000000, "COCO image dimension exceeds native domain");
      return std::uint32_t(n);
    };
    for (const auto i : images) {
      const auto &n = tree.nodes[i];
      tree.keys(n, {"id", "width", "height", "file_name", "license",
                    "flickr_url", "coco_url", "date_captured"});
      dimension(n, "width");
      dimension(n, "height");
      string(tree.get(n, "file_name"));
    }
    for (const auto i : categories) {
      const auto &n = tree.nodes[i];
      tree.keys(n, {"id", "name", "supercategory"});
      const auto value = string(tree.get(n, "name"));
      valid(!value.empty() && value.size() <= 4096,
            "COCO category name exceeds native bound");
      if (const auto *p = tree.find(n, "supercategory"))
        string(*p);
    }
    const auto &image = tree.nodes[image_ids.at(selected_id)];
    const auto w = dimension(image, "width"),
               height = dimension(image, "height");
    const auto pixels = std::uint64_t(w) * height;
    valid(pixels <= (1024u * 1024 - 12) * UINT64_C(8),
          "COCO selected image exceeds QBM1 pixel transport bound");
    Mask mask(w, height, memory, request.cancel);
    for (const auto i : annotations) {
      poll(request.cancel);
      const auto &n = tree.nodes[i];
      tree.keys(n, {"id", "image_id", "category_id", "segmentation", "area",
                    "bbox", "iscrowd"});
      const auto image_id = integer(tree.get(n, "image_id")),
                 category_id = integer(tree.get(n, "category_id"));
      valid(image_ids.count(image_id) && category_ids.count(category_id),
            "COCO annotation has an unresolved image/category ID");
      if (image_id != selected_id)
        continue;
      const auto annotation_id = integer(tree.get(n, "id"));
      const auto crowd = integer(tree.get(n, "iscrowd"));
      valid(crowd <= 1, "COCO iscrowd must be zero or one");
      valid(real(tree.get(n, "area")) >= 0, "COCO source area is negative");
      const auto &bbox = tree.array(tree.get(n, "bbox"));
      valid(bbox.size() == 4, "COCO bbox requires four values");
      for (std::size_t b = 0; b < 4; ++b) {
        const auto value = real(tree.nodes[bbox[b]]);
        valid(b < 2 || value >= 0, "COCO bbox extent is negative");
      }
      const auto &segmentation = tree.get(n, "segmentation");
      mask.segmentation(tree, segmentation);
      h.rows.emplace_back(memory);
      auto &row = h.rows.back();
      row.id = "annotation/" + std::to_string(annotation_id);
      // Category IDs remain distinct even when display names repeat.
      row.classification = "category/" + std::to_string(category_id);
      row.encoding = QPX_FORMAT_GEOMETRY_BINARY_MASK1;
      row.geometry = {'Q', 'B', 'M', 1};
      GeometryEncoder encoder{row.geometry};
      encoder.u32(w);
      encoder.u32(height);
      row.geometry.insert(row.geometry.end(), mask.bits.begin(),
                          mask.bits.end());
      auto &facts = row.facts;
      fact(facts, "coco.profile", contract);
      number(facts, "coco.image_id", image_id);
      number(facts, "coco.category_id", category_id);
      number(facts, "coco.annotation_id", annotation_id);
      number(facts, "coco.iscrowd", crowd);
      fact(facts, "coco.segmentation",
           segmentation.kind == Kind::Array ? "polygon-union"
           : tree.get(segmentation, "counts").kind == Kind::Array
               ? "uncompressed-rle"
               : "compressed-rle");
      fact(facts, "coco.coverage", "origin-zero-level-zero-unit-pixel-cells");
      fact(facts, "coco.category_name",
           string(tree.get(tree.nodes[category_ids.at(category_id)], "name")));
      chunks(facts, "coco.annotation.json", n.raw);
      chunks(facts, "coco.image.json", image.raw);
      chunks(facts, "coco.category.json",
             tree.nodes[category_ids.at(category_id)].raw);
      if (const auto *p = tree.find(tree.root(), "info"))
        chunks(facts, "coco.info.json", p->raw);
      if (const auto *p = tree.find(tree.root(), "licenses"))
        chunks(facts, "coco.licenses.json", p->raw);
    }
    valid(!h.rows.empty(),
          "Selected COCO image has no nonempty instance segmentations");
  }
};
} // namespace
extern "C" QPX_ANNOTATION_MASK_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v5(std::uint32_t abi,
                               qpx_annotation_format_api_v5 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<Definition, 5>::entry(abi, out, error);
}
