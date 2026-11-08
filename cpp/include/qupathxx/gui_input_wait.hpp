#pragma once
namespace qpx {
// Numeric values and Python names are part of the replay API. Rows retain the
// historical Python registration order, which differs from numeric order.
#define QPX_GUI_INPUT_WAIT_VALUES(X) \
    X(TiffReady, 43, "TIFF_READY") \
    X(TiffViewApplied, 44, "TIFF_VIEW_APPLIED") \
    X(RawTaskFinished, 45, "RAW_TASK_FINISHED") \
    X(RawNumpySaved, 46, "RAW_NUMPY_SAVED") \
    X(RawAnalysisKept, 47, "RAW_ANALYSIS_KEPT") \
    X(RawAnalysisVerified, 48, "RAW_ANALYSIS_VERIFIED") \
    X(RawMeasurementsSaved, 49, "RAW_MEASUREMENTS_SAVED") \
    X(RawPixelFinished, 50, "RAW_PIXEL_FINISHED") \
    X(RawPixelTrained, 51, "RAW_PIXEL_TRAINED") \
    X(RawPixelPredicted, 52, "RAW_PIXEL_PREDICTED") \
    X(RawPixelPreviewReady, 53, "RAW_PIXEL_PREVIEW_READY") \
    X(RawPixelKept, 54, "RAW_PIXEL_KEPT") \
    X(RawPixelFileSaved, 55, "RAW_PIXEL_FILE_SAVED") \
    X(RawPixelFileRead, 56, "RAW_PIXEL_FILE_READ") \
    X(RawPixelModelSaved, 57, "RAW_PIXEL_MODEL_SAVED") \
    X(RawPixelModelLoaded, 58, "RAW_PIXEL_MODEL_LOADED") \
    X(RawPixelContoursReady, 59, "RAW_PIXEL_CONTOURS_READY") \
    X(RawPixelContoursPublished, 60, "RAW_PIXEL_CONTOURS_PUBLISHED") \
    X(TmaGridReady, 41, "TMA_GRID_READY") \
    X(TmaMetadataSaved, 42, "TMA_METADATA_SAVED") \
    X(Immediate, 0, "IMMEDIATE") \
    X(TargetReady, 1, "TARGET_READY") \
    X(StartupFinished, 2, "STARTUP_FINISHED") \
    X(TissueReady, 3, "TISSUE_READY") \
    X(TissueContoursReady, 4, "TISSUE_CONTOURS_READY") \
    X(TissueContoursPublished, 5, "TISSUE_CONTOURS_PUBLISHED") \
    X(TissueArchived, 6, "TISSUE_ARCHIVED") \
    X(TissueVerified, 7, "TISSUE_VERIFIED") \
    X(CellsReady, 8, "CELLS_READY") \
    X(CellsPublished, 9, "CELLS_PUBLISHED") \
    X(CellsArchived, 10, "CELLS_ARCHIVED") \
    X(CellsVerified, 11, "CELLS_VERIFIED") \
    X(CellTableSaved, 12, "CELL_TABLE_SAVED") \
    X(FileFingerprintReady, 13, "FILE_FINGERPRINT_READY") \
    X(MeasurementTableSaved, 14, "MEASUREMENT_TABLE_SAVED") \
    X(StainsEstimated, 15, "STAINS_ESTIMATED") \
    X(StainsApplied, 16, "STAINS_APPLIED") \
    X(BatchFinished, 17, "BATCH_FINISHED") \
    X(WindowReady, 18, "WINDOW_READY") \
    X(ObjectClassifierTrained, 19, "CLASSIFIER_TRAINED") \
    X(ObjectClassifierPredicted, 20, "CLASSIFIER_PREDICTED") \
    X(ObjectClassifierApplied, 21, "CLASSIFIER_APPLIED") \
    X(ClassifierModelSaved, 22, "CLASSIFIER_MODEL_SAVED") \
    X(ClassifierModelLoaded, 23, "CLASSIFIER_MODEL_LOADED") \
    X(ClassifierTrainingRetained, 24, "CLASSIFIER_TRAINING_RETAINED") \
    X(ClassifierRecordLoaded, 25, "CLASSIFIER_RECORD_LOADED") \
    X(PixelClassifierTrained, 26, "PIXEL_TRAINED") \
    X(PixelClassifierPredicted, 27, "PIXEL_PREDICTED") \
    X(PixelPreviewReady, 28, "PIXEL_PREVIEW_READY") \
    X(PixelModelSaved, 29, "PIXEL_MODEL_SAVED") \
    X(PixelModelLoaded, 30, "PIXEL_MODEL_LOADED") \
    X(PixelContoursReady, 31, "PIXEL_CONTOURS_READY") \
    X(PixelContoursPublished, 32, "PIXEL_CONTOURS_PUBLISHED") \
    X(PixelRecordKept, 33, "PIXEL_RECORD_KEPT") \
    X(PixelRecordModelLoaded, 34, "PIXEL_RECORD_MODEL_LOADED") \
    X(PixelHistoricalPreviewReady, 35, "PIXEL_HISTORICAL_PREVIEW_READY") \
    X(PixelStreamFinished, 36, "PIXEL_STREAM_FINISHED") \
    X(PixelStreamReadReady, 37, "PIXEL_STREAM_READ_READY") \
    X(PixelStreamReadFinished, 38, "PIXEL_STREAM_READ_FINISHED") \
    X(PixelStreamVerified, 39, "PIXEL_STREAM_VERIFIED") \
    X(PixelStreamLinked, 40, "PIXEL_STREAM_LINKED") \
    X(SpatialReady, 61, "SPATIAL_READY") \
    X(SpatialKept, 62, "SPATIAL_KEPT") \
    X(SpatialVerified, 63, "SPATIAL_VERIFIED") \
    X(SpatialExported, 64, "SPATIAL_EXPORTED") \
    X(StainModelFinished, 65, "STAIN_MODEL_FINISHED")

enum class GuiInputWait {
#define QPX_GUI_INPUT_WAIT_ENUM(name,value,python_name) name=value,
    QPX_GUI_INPUT_WAIT_VALUES(QPX_GUI_INPUT_WAIT_ENUM)
#undef QPX_GUI_INPUT_WAIT_ENUM
};
namespace detail {
struct GuiInputWaitDescriptor {
    GuiInputWait value;
    const char* python_name;
};
inline constexpr GuiInputWaitDescriptor gui_input_wait_values[]={
#define QPX_GUI_INPUT_WAIT_DESCRIPTOR(name,value,python_name) {GuiInputWait::name,python_name},
    QPX_GUI_INPUT_WAIT_VALUES(QPX_GUI_INPUT_WAIT_DESCRIPTOR)
#undef QPX_GUI_INPUT_WAIT_DESCRIPTOR
};
constexpr bool valid_gui_input_wait(GuiInputWait value) {
    for(const auto& condition:gui_input_wait_values) if(condition.value==value) return true;
    return false;
}
}
#undef QPX_GUI_INPUT_WAIT_VALUES
}
