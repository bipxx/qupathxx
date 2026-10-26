#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
#include "qupathxx/provenance.hpp"
namespace qpx {
// Immutable historical data. Decoding never restores a request or live authority.
class RawPixelArchive {
public:
    const std::shared_ptr<const RawPixelTrainingTable>& table() const {return table_;}
    const std::shared_ptr<const RawPixelClassification>& result() const {return result_;}
    const std::shared_ptr<const RawPixelClassifier>& model() const {return model_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    const ScientificGrid& grid() const;
    std::string purpose() const {return table_?(model_?"training":"sampling"):"prediction";}
    const std::vector<std::uint8_t>& bytes() const {return bytes_;}
    const std::string& sha256() const {return sha_;}
    std::size_t payload_bytes() const {return payload_;}
    static std::shared_ptr<const RawPixelArchive> from_bytes(const std::vector<std::uint8_t>&,const std::atomic<bool>* cancelled=nullptr);
private:
    RawPixelArchive()=default;
    std::shared_ptr<const RawPixelTrainingTable> table_;
    std::shared_ptr<const RawPixelClassification> result_;
    std::shared_ptr<const RawPixelClassifier> model_;
    std::shared_ptr<const RawPixelPredictionRequest::Data> prediction_;
    std::shared_ptr<const char> capture_;
    std::vector<std::uint8_t> bytes_;
    std::string sha_;std::size_t payload_=0;
    friend struct RawPixelArchiveCodec;
    friend class Document;
};
std::shared_ptr<const RawPixelArchive> archive_raw_pixel_training(std::shared_ptr<const RawPixelTrainingTable>,
    std::shared_ptr<const RawPixelClassifier> model={},const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const RawPixelArchive> archive_raw_pixel_prediction(const RawPixelPredictionRequest&,
    std::shared_ptr<const RawPixelClassification>,const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const RawPixelArchive> load_raw_pixel_archive(const std::string&,const std::atomic<bool>* cancelled=nullptr);
void save_raw_pixel_archive(const RawPixelArchive&,const std::string&,bool replace_existing=false,const std::atomic<bool>* cancelled=nullptr);
struct RawPixelRecord {
    std::uint64_t id=0,captured_next_id=0;
    std::string name;
    std::shared_ptr<const RawPixelArchive> archive;
    std::size_t payload_bytes() const {return sizeof(*this)+name.capacity()+(archive?archive->payload_bytes():0);}
};
}
