#pragma once
#include "qupathxx/raw_pixel_archive.hpp"
namespace qpx {
struct RawPixelArchiveCodec {
    static std::shared_ptr<const RawPixelArchive> training(std::shared_ptr<const RawPixelTrainingTable>,std::shared_ptr<const RawPixelClassifier>,const std::atomic<bool>*);
    static std::shared_ptr<const RawPixelArchive> prediction(const RawPixelPredictionRequest&,std::shared_ptr<const RawPixelClassification>,const std::atomic<bool>*);
    static std::shared_ptr<const RawPixelArchive> decode(const std::uint8_t*,std::size_t,const std::atomic<bool>*);
    static std::vector<std::uint8_t> encode_record(const RawPixelRecord&,const std::atomic<bool>*);
    static std::shared_ptr<const RawPixelRecord> decode_record(const std::uint8_t*,std::size_t,const ImageMetadata&,const std::atomic<bool>*);
    static void validate(const RawPixelArchive&,const std::atomic<bool>*);
    static void seal(RawPixelArchive&,std::vector<std::uint8_t>);
};
}
