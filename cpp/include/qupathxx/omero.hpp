#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "qupathxx/raw_image.hpp"
namespace qpx {
struct OmeroLimits {
    std::size_t json_bytes = 2u * 1024 * 1024, preview_bytes = 16u * 1024 * 1024,
                max_preview_pixels = 1024u * 1024;
    std::uint32_t timeout_ms = 15000;
    void validate() const;
};
// Bounded GET-only transport. No credentials, cookies or live Document pointers
// belong to the protocol. Native implementations must disable automatic redirects
// and implicit authentication; the client admits same-server redirects explicitly.
struct OmeroHttpResponse {
    std::uint32_t status = 0;
    std::string content_type, location;
    std::vector<std::uint8_t> body;
};
class OmeroTransport {
  public:
    virtual ~OmeroTransport() = default;
    virtual OmeroHttpResponse get(const std::string& https_url, std::size_t byte_limit,
                                  std::uint32_t timeout_ms, const std::atomic<bool>* cancelled) = 0;
};
std::shared_ptr<OmeroTransport> native_omero_transport();
struct OmeroObject {
    std::uint64_t id = 0;
    std::string name;
};
struct OmeroPage {
    std::vector<OmeroObject> objects;
    std::uint64_t offset = 0, total = 0;
};
struct OmeroQuantity {
    double value = 0;
    std::string unit;
};
struct OmeroImageInfo {
    std::uint64_t id = 0, pixels_id = 0, width = 0, height = 0;
    std::uint32_t channels = 0, z_planes = 0, timepoints = 0;
    std::string name, pixel_type, metadata_sha256;
    // Only an explicit anonymous NGFF externalInfo association is admitted.
    std::optional<std::string> ngff_url;
    std::optional<OmeroQuantity> physical_x, physical_y;
};
struct OmeroRawOptions {
    // Native Zarr sampling workspace; codec opening has its own bounded host
    // workspace. These are operation budgets, not a process-RSS guarantee.
    std::size_t work_bytes = 256u * 1024 * 1024, transfer_bytes = 64u * 1024 * 1024;
    std::uint32_t max_chunks = 256;
    void validate() const;
};
class OmeroRawBlock {
  public:
    const RawPixelBuffer& pixels() const;
    const RawImageMetadata& metadata() const;
    const RawRegionRequest& request() const;
    const OmeroImageInfo& image() const;
    const std::string& receipt_json() const;
    const std::string& sha256() const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit OmeroRawBlock(std::shared_ptr<const Data>);
    friend class OmeroClient;
};
class OmeroRois;
struct OmeroRoiOptions {
    std::optional<std::uint32_t> z, t;
    std::size_t max_mask_pixels = 16u * 1024 * 1024, mask_bytes = 64u * 1024 * 1024;
};
class OmeroPreview {
  public:
    const OmeroImageInfo& image() const;
    const std::string& server() const;
    const std::vector<std::uint8_t>& jpeg() const;
    const std::string& sha256() const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit OmeroPreview(std::shared_ptr<const Data>);
    friend class OmeroClient;
};
// Public OMERO.web JSON API v0 profile. Each client has one native owner thread.
// Metadata and rendered thumbnails are distinct from original raw samples.
class OmeroClient {
  public:
    explicit OmeroClient(std::string https_server, std::shared_ptr<OmeroTransport>,
                         OmeroLimits = {});
    ~OmeroClient();
    OmeroClient(const OmeroClient&) = delete;
    void connect(const std::atomic<bool>* cancelled = nullptr);
    const std::string& server() const;
    OmeroPage projects(std::uint64_t offset = 0, std::uint32_t limit = 100,
                       const std::atomic<bool>* cancelled = nullptr);
    OmeroPage datasets(std::uint64_t project, std::uint64_t offset = 0, std::uint32_t limit = 100,
                       const std::atomic<bool>* cancelled = nullptr);
    OmeroPage images(std::uint64_t dataset, std::uint64_t offset = 0, std::uint32_t limit = 100,
                     const std::atomic<bool>* cancelled = nullptr);
    OmeroImageInfo image(std::uint64_t id, const std::atomic<bool>* cancelled = nullptr);
    std::shared_ptr<const OmeroPreview> preview(std::uint64_t id,
                                                const std::atomic<bool>* cancelled = nullptr);
    // Level-zero, in-bounds blocks only. Never rendered pixels or a partial
    // store presented as a complete image. Uses the native NGFF authority.
    std::shared_ptr<const OmeroRawBlock> raw(std::uint64_t, const RawRegionRequest&,
                                             OmeroRawOptions = {},
                                             const std::atomic<bool>* cancelled = nullptr);
    std::shared_ptr<const OmeroRois> rois(std::uint64_t, OmeroRoiOptions = {},
                                          const std::atomic<bool>* cancelled = nullptr);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<std::uint8_t> json_get(const std::string&, const std::atomic<bool>*);
    OmeroHttpResponse store_get(const std::string& base, const std::string& key, std::size_t cap,
                                const std::atomic<bool>*);
    std::vector<std::uint8_t> mask_get(std::uint64_t shape, std::size_t cap,
                                       const std::atomic<bool>*);
};
std::string omero_image_json(const OmeroImageInfo&);
void save_omero_preview(const OmeroPreview&, const std::string& new_jpeg,
                        const std::atomic<bool>* cancelled = nullptr);
// Uses the existing admitted JPEG reader, then embeds an uncalibrated RGB preview.
// The project cannot expose the server's original quantitative raw capability.
void save_omero_preview_project(const OmeroPreview&, const std::string& new_project,
                                OmeroLimits = {}, const std::atomic<bool>* cancelled = nullptr);
// Owned HWC .npy plus an independently exported exact source/selection receipt.
void save_omero_raw(const OmeroRawBlock&, const std::string& new_numpy,
                    const std::atomic<bool>* cancelled = nullptr);
void save_omero_raw_receipt(const OmeroRawBlock&, const std::string& new_json,
                            const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
