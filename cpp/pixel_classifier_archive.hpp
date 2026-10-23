#pragma once
#include "qupathxx/pixel_classifier_archive.hpp"
namespace qpx {
struct PixelClassifierArchiveCodec {
    static std::shared_ptr<const PixelClassifierArchive> training(std::shared_ptr<const PixelTrainingTable>,const PixelClassifier&,const std::atomic<bool>*);
    static std::shared_ptr<const PixelClassifierArchive> prediction(std::shared_ptr<const PixelPredictionResult>,const std::atomic<bool>*);
    static std::shared_ptr<const PixelClassifierArchive> decode(const std::uint8_t*,std::size_t,const std::atomic<bool>*);
    static std::vector<std::uint8_t> encode(const PixelClassifierRecord&,const std::atomic<bool>*);
    static std::shared_ptr<const PixelClassifierRecord> record(const std::uint8_t*,std::size_t,const ImageMetadata&,const std::atomic<bool>*);
    static void seal(PixelClassifierArchive&,std::vector<std::uint8_t>);
};
}
