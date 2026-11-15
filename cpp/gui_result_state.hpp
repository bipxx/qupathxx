#pragma once
#include "gui_i18n.hpp"
#include <string>
#include <string_view>

// Presentation only: native tokens and identities remain unchanged. Exact, case-sensitive parsing.
namespace qpx::ui {
inline const char* session_model_warning() {return "The current model has not been kept or saved.";}
inline const char* kept_samples_hint() {return i18n::text("Historical samples from the kept run. Geometry edits do not recompute these values.");}
enum class RunState {Kept,Added,Saved,Reused,Partial,Unresolved,Unsupported,Idle,Running,Ready,Stale,Cancelling,Cancelled,Failed,Verified,Historical,Unknown};
enum class RecordState {NoFeatures,GeometryUnavailable,GeometryChanged,ContextChanged,CalibrationChanged,ObjectsOrCalibrationChanged,ArchiveMissing,InvalidLabel,Removed,Current,Unverified,ObjectChanged,SourceChanged,SourceClosed,VerificationFailed,Unknown};
enum class RecoveryState {Unsaved,Changed,Matching,Relocated,Unreadable,Missing,Unknown};
enum class SourceMetadataState {Unobserved,IncompatibleScheme,DifferentFile,VersionChanged,MatchingMetadata,MatchingPrimaryMetadata,Unknown};
enum class ExportState {Cancelling,Stale,Idle,Writing,Clearing,Succeeded,Cancelled,Failed,Unknown};

inline RunState parse_run_state(std::string_view token) {
    if(token=="idle" || token=="waiting" || token=="not_run") return RunState::Idle;
    if(token=="running" || token=="opening" || token=="loading" || token=="reading" || token=="changing" || token=="preparing" || token=="tracing" || token=="producing" || token=="opening replay source") return RunState::Running;
    if(token=="ready" || token=="applied" || token=="preview" || token=="trained" || token=="predicted" || token=="contours_ready" || token=="read" || token=="loaded" || token=="block_ready" || token=="complete" || token=="completed" || token=="succeeded" || token=="imported" || token=="delivered" || token=="preview ready") return RunState::Ready;
    if(token=="reused") return RunState::Reused;
    if(token=="stale") return RunState::Stale;
    if(token=="cancelling") return RunState::Cancelling;
    if(token=="cancelled") return RunState::Cancelled;
    if(token=="failed" || token=="error" || token=="verification_failed") return RunState::Failed;
    if(token=="verified") return RunState::Verified;
    if(token=="historical" || token=="historical_contours_ready" || token=="detached") return RunState::Historical;
    if(token=="kept" || token=="retained") return RunState::Kept;
    if(token=="published") return RunState::Added;
    if(token=="saved") return RunState::Saved;
    if(token=="partial") return RunState::Partial;
    if(token=="unresolved") return RunState::Unresolved;
    if(token=="unsupported") return RunState::Unsupported;
    return RunState::Unknown;
}

inline RecordState parse_record_state(std::string_view token) {
    if(token=="current") return RecordState::Current;
    if(token=="unverified") return RecordState::Unverified;
    if(token=="object_changed") return RecordState::ObjectChanged;
    if(token=="source_changed") return RecordState::SourceChanged;
    if(token=="source_closed") return RecordState::SourceClosed;
    if(token=="verification_failed") return RecordState::VerificationFailed;
    if(token=="no_features") return RecordState::NoFeatures;
    if(token=="geometry_unavailable") return RecordState::GeometryUnavailable;
    if(token=="geometry_changed") return RecordState::GeometryChanged;
    if(token=="context_changed") return RecordState::ContextChanged;
    if(token=="calibration_changed") return RecordState::CalibrationChanged;
    if(token=="objects_or_calibration_changed") return RecordState::ObjectsOrCalibrationChanged;
    if(token=="archive_missing") return RecordState::ArchiveMissing;
    if(token=="invalid_label") return RecordState::InvalidLabel;
    if(token=="removed") return RecordState::Removed;
    return RecordState::Unknown;
}

inline RecoveryState parse_recovery_state(std::string_view token) {
    if(token=="unsaved") return RecoveryState::Unsaved;
    if(token=="changed") return RecoveryState::Changed;
    if(token=="matching") return RecoveryState::Matching;
    if(token=="relocated") return RecoveryState::Relocated;
    if(token=="unreadable") return RecoveryState::Unreadable;
    if(token=="missing") return RecoveryState::Missing;
    return RecoveryState::Unknown;
}

inline SourceMetadataState parse_source_metadata_state(std::string_view token) {
    if(token=="unobserved") return SourceMetadataState::Unobserved;
    if(token=="incompatible_scheme") return SourceMetadataState::IncompatibleScheme;
    if(token=="different_file") return SourceMetadataState::DifferentFile;
    if(token=="version_changed") return SourceMetadataState::VersionChanged;
    if(token=="matching_metadata") return SourceMetadataState::MatchingMetadata;
    if(token=="matching_primary_metadata") return SourceMetadataState::MatchingPrimaryMetadata;
    return SourceMetadataState::Unknown;
}

inline ExportState parse_export_state(std::string_view token) {
    if(token=="clearing") return ExportState::Clearing;
    if(token=="idle") return ExportState::Idle;
    if(token=="writing" || token=="running" || token=="saving" || token=="exporting") return ExportState::Writing;
    if(token=="saved" || token=="succeeded" || token=="ready" || token=="reused" || token=="detached") return ExportState::Succeeded;
    if(token=="cancelled") return ExportState::Cancelled;
    if(token=="failed" || token=="error") return ExportState::Failed;
    if(token=="cancelling") return ExportState::Cancelling;
    if(token=="stale") return ExportState::Stale;
    return ExportState::Unknown;
}

inline const char* caption(RunState state) {
    switch(state) {
    case RunState::Unsupported:return i18n::text("Unsupported input");
    case RunState::Unresolved:return i18n::text("Needs resolution");
    case RunState::Partial:return i18n::text("Partially complete");
    case RunState::Reused:return i18n::text("Reused");
    case RunState::Saved:return i18n::text("Saved");
    case RunState::Added:return i18n::text("Added");
    case RunState::Kept:return i18n::text("Kept");
    case RunState::Idle:return i18n::text("Not started");
    case RunState::Running:return i18n::text("Running");
    case RunState::Ready:return i18n::text("Ready");
    case RunState::Stale:return i18n::text("Inputs changed");
    case RunState::Cancelling:return i18n::text("Cancelling");
    case RunState::Cancelled:return i18n::text("Cancelled");
    case RunState::Failed:return i18n::text("Work failed");
    case RunState::Verified:return i18n::text("Verified for this session");
    case RunState::Historical:return i18n::text("Historical result");
    case RunState::Unknown:return i18n::text("Unknown status (%s)");
    }
    return i18n::text("Unknown status (%s)");
}

inline const char* caption(RecordState state) {
    switch(state) {
    case RecordState::Removed:return i18n::text("Removed");
    case RecordState::InvalidLabel:return i18n::text("Invalid feature label");
    case RecordState::ArchiveMissing:return i18n::text("Kept record missing");
    case RecordState::ObjectsOrCalibrationChanged:return i18n::text("Objects or calibration changed");
    case RecordState::CalibrationChanged:return i18n::text("Calibration changed");
    case RecordState::ContextChanged:return i18n::text("Context changed");
    case RecordState::GeometryChanged:return i18n::text("Geometry changed");
    case RecordState::GeometryUnavailable:return i18n::text("Geometry unavailable");
    case RecordState::NoFeatures:return i18n::text("No linked features");
    case RecordState::Current:return i18n::text("Verified for this session");
    case RecordState::Unverified:return i18n::text("Needs recheck");
    case RecordState::ObjectChanged:return i18n::text("Object changed");
    case RecordState::SourceChanged:return i18n::text("Source changed");
    case RecordState::SourceClosed:return i18n::text("Image closed");
    case RecordState::VerificationFailed:return i18n::text("Recheck failed");
    case RecordState::Unknown:return i18n::text("Unknown status (%s)");
    }
    return i18n::text("Unknown status (%s)");
}

inline const char* caption(RecoveryState state) {
    switch(state) {
    case RecoveryState::Unsaved:return i18n::text("No saved project");
    case RecoveryState::Changed:return i18n::text("Saved project changed");
    case RecoveryState::Matching:return i18n::text("Saved project matches");
    case RecoveryState::Relocated:return i18n::text("Saved project relocated");
    case RecoveryState::Unreadable:return i18n::text("Saved project unreadable");
    case RecoveryState::Missing:return i18n::text("Saved project missing");
    case RecoveryState::Unknown:return i18n::text("Unknown status (%s)");
    }
    return i18n::text("Unknown status (%s)");
}

inline const char* caption(SourceMetadataState state) {
    switch(state) {
    case SourceMetadataState::Unobserved:return i18n::text("Source metadata unobserved");
    case SourceMetadataState::IncompatibleScheme:return i18n::text("Incompatible identity scheme");
    case SourceMetadataState::DifferentFile:return i18n::text("Different file");
    case SourceMetadataState::VersionChanged:return i18n::text("File version changed");
    case SourceMetadataState::MatchingMetadata:return i18n::text("File metadata matches");
    case SourceMetadataState::MatchingPrimaryMetadata:return i18n::text("Primary file metadata matches");
    case SourceMetadataState::Unknown:return i18n::text("Unknown status (%s)");
    }
    return i18n::text("Unknown status (%s)");
}

inline const char* caption(ExportState state) {
    switch(state) {
    case ExportState::Stale:return i18n::text("Inputs changed");
    case ExportState::Cancelling:return i18n::text("Cancelling");
    case ExportState::Idle:return i18n::text("Not started");
    case ExportState::Clearing:return i18n::text("Removing recovery file");
    case ExportState::Writing:return i18n::text("Writing");
    case ExportState::Succeeded:return i18n::text("Saved");
    case ExportState::Cancelled:return i18n::text("Cancelled");
    case ExportState::Failed:return i18n::text("Export failed");
    case ExportState::Unknown:return i18n::text("Unknown status (%s)");
    }
    return i18n::text("Unknown status (%s)");
}

// Unknown is a format key in the enum-only interface. This overload supplies the
// raw token safely (including percent signs); callers must not print the key alone.
template<class State> inline std::string caption(State state,std::string_view raw) {
    std::string result=caption(state);
    if(state==State::Unknown)result.replace(result.find("%s"),2,raw.data(),raw.size());
    return result;
}
}
