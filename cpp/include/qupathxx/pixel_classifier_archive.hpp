#pragma once
#include "qupathxx/pixel_prediction.hpp"
namespace qpx {
struct PixelAnnotationSnapshot {
    std::uint64_t id=0,revision=0;
    ObjectKind kind=ObjectKind::Annotation;
    std::string class_name;
    Roi geometry;
};
// Immutable, worker-prepared numeric artifact. Decoding restores no live authority.
class PixelClassifierArchive : public std::enable_shared_from_this<PixelClassifierArchive> {
public:
    const std::shared_ptr<const PixelTrainingTable>& table() const {return table_;}
    const std::shared_ptr<const PixelPredictionResult>& result() const {return result_;}
    const std::shared_ptr<const PixelClassifier>& model() const {return model_;}
    const std::vector<PixelAnnotationSnapshot>& annotations() const {return annotations_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    std::string purpose() const {return table_?"training":"prediction";}
    const std::string& sha256() const {return sha_;}
    const std::vector<std::uint8_t>& bytes() const {return bytes_;}
    std::size_t payload_bytes() const {return payload_;}
    static std::shared_ptr<const PixelClassifierArchive> from_bytes(const std::vector<std::uint8_t>&);
private:
    PixelClassifierArchive()=default;
    std::shared_ptr<const PixelTrainingTable> table_;
    std::shared_ptr<const PixelPredictionResult> result_;
    std::shared_ptr<const PixelClassifier> model_;
    std::vector<PixelAnnotationSnapshot> annotations_;
    std::vector<std::uint8_t> bytes_;
    std::string sha_;
    std::size_t payload_=0;
    friend struct PixelClassifierArchiveCodec;
};
std::shared_ptr<const PixelClassifierArchive> archive_pixel_training(std::shared_ptr<const PixelTrainingTable>,const PixelClassifier&,const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const PixelClassifierArchive> archive_pixel_prediction(std::shared_ptr<const PixelPredictionResult>,const std::atomic<bool>* cancelled=nullptr);
struct PixelClassifierRecord : public std::enable_shared_from_this<PixelClassifierRecord> {
    std::uint64_t id=0,captured_next_id=0;
    std::string name;
    std::shared_ptr<const PixelClassifierArchive> archive;
    std::size_t payload_bytes() const;
};
}
