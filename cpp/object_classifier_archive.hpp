#pragma once
#include "qupathxx/object_classifier_archive.hpp"
namespace qpx {
struct ObjectClassifierArchiveCodec {
    static std::shared_ptr<const ObjectClassifierArchive> prepare(std::shared_ptr<const ObjectFeatureTable>,const ForestModel&,
        std::shared_ptr<const ObjectClassificationResult>,const std::atomic<bool>*);
    static std::vector<std::uint8_t> encode(const ObjectClassifierRecord&,const std::atomic<bool>*);
    static std::shared_ptr<const ObjectClassifierRecord> decode(const std::uint8_t*,std::size_t,const ImageMetadata&,const std::atomic<bool>*);
};
}
