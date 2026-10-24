#pragma once

namespace qpx::detail {
// Session-owned UI storage. Keep the fixed capacities/array types used by
// InputText and path validation, without consuming the native caller's stack.
struct GuiTextBuffers {
    GuiTextBuffers() = default;
    GuiTextBuffers(const GuiTextBuffers&) = delete;
    GuiTextBuffers& operator=(const GuiTextBuffers&) = delete;

    char fingerprint_path[32769]{}, fingerprint_output[32769]{};
    char measurement_path[32769]{}, cell_table_path[32769]{};
    char project_path[32769]{};
    char autosave_path[32769]{}, recovery_path[32769]{}, recovery_base_path[32769]{};
    char project_open_path[32769]{}, project_image_path[32769]{};
    char project_image_name[4097]{}, project_filter[257]{};
    char interchange_path[32769]{};
    char draft_name[4097]{}, draft_class[4097]{}, class_name[4097]{};
};
}
