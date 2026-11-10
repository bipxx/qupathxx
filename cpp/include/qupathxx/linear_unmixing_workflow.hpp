#pragma once
#include "qupathxx/linear_unmixing.hpp"
#include "qupathxx/dark_flat_workflow.hpp"
namespace qpx {
struct LinearUnmixRecipe {
    DarkFlatSourceReference signal;
    // Empty for original input; otherwise references a retained correction.
    std::string correction_record;
    LinearMixingMatrix matrix;
    std::string algorithm="qpx.linear-unmixing-svd/1";
    std::string operator_sha256,input_source_sha256,derived_source_sha256;
    void validate() const;
    std::string sha256() const;
    std::size_t payload_bytes() const;
    ImageDataValue to_value() const;
    static LinearUnmixRecipe from_value(const ImageDataValue&);
};
class LinearUnmixRequest {
public:
    LinearUnmixRequest(const Document&,LinearMixingMatrix,const std::atomic<bool>* cancelled=nullptr);
    LinearUnmixRequest(const Document&,const DarkFlatRequest&,LinearMixingMatrix,const std::atomic<bool>* cancelled=nullptr);
    // Reuse a model prepared on a native worker; owner capture does no SVD.
    LinearUnmixRequest(const Document&,LinearUnmixModel,const std::atomic<bool>* cancelled=nullptr);
    LinearUnmixRequest(const Document&,const DarkFlatRequest&,LinearUnmixModel,const std::atomic<bool>* cancelled=nullptr);
    bool matches(const Document&) const;
    void require_current(const Document&,const std::atomic<bool>* cancelled=nullptr) const;
    const LinearUnmixRecipe& recipe() const;
    // Workers may read this immutable native source; Document stays on owner.
    const std::shared_ptr<const ChannelSource>& source() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
};
std::string keep_linear_unmix_recipe(Document&,const LinearUnmixRequest&,const std::atomic<bool>* cancelled=nullptr);
std::vector<std::string> linear_unmix_recipe_ids(const Document&);
LinearUnmixRecipe load_linear_unmix_recipe(const Document&,const std::string&);
void remove_linear_unmix_recipe(Document&,const std::string&);
LinearUnmixRequest replay_linear_unmix_recipe(const Document&,const std::string&,const std::atomic<bool>* cancelled=nullptr);
// Reuse an already restored correction instead of opening its fields again.
LinearUnmixRequest restore_linear_unmix_recipe(const Document&,const LinearUnmixRecipe&,
    const std::optional<DarkFlatRequest>& correction={},const std::atomic<bool>* cancelled=nullptr);
LinearUnmixRequest restore_linear_unmix_recipe(const Document&,const LinearUnmixRecipe&,LinearUnmixModel,
    const std::optional<DarkFlatRequest>& correction={},const std::atomic<bool>* cancelled=nullptr);
bool linear_unmix_recipes_reference_correction(const Document&,const std::string&);
}
