#pragma once
#include "qupathxx/registration.hpp"

namespace qpx {
enum class LandmarkModel { Translation, Similarity, Affine };
struct LandmarkPair {std::string id;Point source,target;bool held_out=false;};
struct LandmarkFitOptions {
    LandmarkModel model=LandmarkModel::Affine;
    bool reflect_source_y=false; // explicit similarity reflection, never guessed
    CoordinateUnit target_unit=CoordinateUnit::Pixel;
    void validate() const;
};
struct LandmarkResidual {Point delta;double distance=0;};
class LandmarkFit {
public:
    const LandmarkFitOptions& options() const;
    const std::vector<LandmarkPair>& landmarks() const;
    const std::vector<LandmarkResidual>& residuals() const;
    const Affine2D& transform() const;
    double training_rmse() const;
    std::optional<double> held_out_rmse() const;
    const std::string& sha256() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    explicit LandmarkFit(std::shared_ptr<const Data>);
    friend LandmarkFit fit_landmarks(std::vector<LandmarkPair>,LandmarkFitOptions,std::size_t,const std::atomic<bool>*);
    friend std::shared_ptr<const LandmarkFit> stored_landmark_fit(std::shared_ptr<const ImageDataProperties>,const std::string&);
};
// Normalized native least squares; affine uses Householder QR with a1e-12
// relative rank threshold. Held-out points never affect the fitted coefficients.
// At most1024 finite, uniquely named pairs; source/target units are explicit.
LandmarkFit fit_landmarks(std::vector<LandmarkPair>,LandmarkFitOptions={},
    std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// One native undo command stores definition, raw pairs, method/residual evidence
// and active registration. Neither source geometry nor counts is rewritten.
void accept_landmark_registration(Document&,const std::string& id,const RegistrationRequest&,const LandmarkFit&,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const LandmarkFit> stored_landmark_fit(std::shared_ptr<const ImageDataProperties>,const std::string& id);
std::optional<std::string> active_registration_id(std::shared_ptr<const ImageDataProperties>);
std::optional<RegistrationField> active_registration_field(const Document&,ExpressionDataset,
    std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
}
