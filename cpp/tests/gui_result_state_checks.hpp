#pragma once
#include "../gui_result_state.hpp"
#include <cstdio>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace result_state_checks {
using namespace qpx::ui;
struct RunToken {const char* token;RunState expected;};
inline constexpr RunToken run_tokens[]={
    {"idle",RunState::Idle},
    {"waiting",RunState::Idle},
    {"not_run",RunState::Idle},
    {"running",RunState::Running},
    {"opening",RunState::Running},
    {"loading",RunState::Running},
    {"reading",RunState::Running},
    {"changing",RunState::Running},
    {"preparing",RunState::Running},
    {"tracing",RunState::Running},
    {"producing",RunState::Running},
    {"opening replay source",RunState::Running},
    {"ready",RunState::Ready},
    {"preview",RunState::Ready},
    {"trained",RunState::Ready},
    {"predicted",RunState::Ready},
    {"contours_ready",RunState::Ready},
    {"read",RunState::Ready},
    {"loaded",RunState::Ready},
    {"applied",RunState::Ready},
    {"published",RunState::Added},
    {"retained",RunState::Kept},
    {"kept",RunState::Kept},
    {"block_ready",RunState::Ready},
    {"complete",RunState::Ready},
    {"completed",RunState::Ready},
    {"succeeded",RunState::Ready},
    {"saved",RunState::Saved},
    {"imported",RunState::Ready},
    {"delivered",RunState::Ready},
    {"reused",RunState::Reused},
    {"preview ready",RunState::Ready},
    {"stale",RunState::Stale},
    {"partial",RunState::Partial},
    {"unresolved",RunState::Unresolved},
    {"cancelling",RunState::Cancelling},
    {"cancelled",RunState::Cancelled},
    {"failed",RunState::Failed},
    {"error",RunState::Failed},
    {"unsupported",RunState::Unsupported},
    {"verification_failed",RunState::Failed},
    {"verified",RunState::Verified},
    {"historical",RunState::Historical},
    {"historical_contours_ready",RunState::Historical},
    {"detached",RunState::Historical},
};
struct RecordToken {const char* token;RecordState expected;};
inline constexpr RecordToken record_tokens[]={
    {"removed",RecordState::Removed},
    {"current",RecordState::Current},
    {"unverified",RecordState::Unverified},
    {"no_features",RecordState::NoFeatures},
    {"geometry_unavailable",RecordState::GeometryUnavailable},
    {"object_changed",RecordState::ObjectChanged},
    {"context_changed",RecordState::ContextChanged},
    {"objects_or_calibration_changed",RecordState::ObjectsOrCalibrationChanged},
    {"geometry_changed",RecordState::GeometryChanged},
    {"source_changed",RecordState::SourceChanged},
    {"calibration_changed",RecordState::CalibrationChanged},
    {"source_closed",RecordState::SourceClosed},
    {"verification_failed",RecordState::VerificationFailed},
    {"archive_missing",RecordState::ArchiveMissing},
    {"invalid_label",RecordState::InvalidLabel},
};
struct RecoveryToken {const char* token;RecoveryState expected;};
inline constexpr RecoveryToken recovery_tokens[]={
    {"unsaved",RecoveryState::Unsaved},
    {"changed",RecoveryState::Changed},
    {"matching",RecoveryState::Matching},
    {"relocated",RecoveryState::Relocated},
    {"unreadable",RecoveryState::Unreadable},
    {"missing",RecoveryState::Missing},
};
struct SourceMetadataToken {const char* token;SourceMetadataState expected;};
inline constexpr SourceMetadataToken source_metadata_tokens[]={
    {"unobserved",SourceMetadataState::Unobserved},
    {"incompatible_scheme",SourceMetadataState::IncompatibleScheme},
    {"different_file",SourceMetadataState::DifferentFile},
    {"version_changed",SourceMetadataState::VersionChanged},
    {"matching_metadata",SourceMetadataState::MatchingMetadata},
    {"matching_primary_metadata",SourceMetadataState::MatchingPrimaryMetadata},
};
struct ExportToken {const char* token;ExportState expected;};
inline constexpr ExportToken export_tokens[]={
    {"stale",ExportState::Stale},
    {"cancelling",ExportState::Cancelling},
    {"idle",ExportState::Idle},
    {"writing",ExportState::Writing},
    {"running",ExportState::Writing},
    {"saving",ExportState::Writing},
    {"exporting",ExportState::Writing},
    {"clearing",ExportState::Clearing},
    {"saved",ExportState::Succeeded},
    {"succeeded",ExportState::Succeeded},
    {"ready",ExportState::Succeeded},
    {"reused",ExportState::Succeeded},
    {"detached",ExportState::Succeeded},
    {"cancelled",ExportState::Cancelled},
    {"failed",ExportState::Failed},
    {"error",ExportState::Failed},
};
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class State> void check_caption(State state) {
    const std::string key=caption(state);bool found=false;
    for(const auto& entry:i18n::catalogue)if(key==entry.english) {
        found=true;for(const auto* translated:entry.translated)require(translated && *translated,"Missing caption translation");
    }
    require(found,"Caption absent from catalogue");
}
template<class Table,class Parser> void tokens(const Table& table,Parser parse) {
    for(const auto& row:table) {
        require(parse(row.token)==row.expected,"Inventory token mapped incorrectly");
        require(std::string(caption(row.expected))!=row.token,"Raw token caption");
        require(parse(std::string(" ")+row.token)==decltype(row.expected)::Unknown,"Leading whitespace folded");
        require(parse(std::string(row.token)+" ")==decltype(row.expected)::Unknown,"Trailing whitespace folded");
        std::string upper=row.token;upper[0]=char(std::toupper(static_cast<unsigned char>(upper[0])));
        require(parse(upper)==decltype(row.expected)::Unknown,"Case folded");
    }
    const std::string raw="future_%s_token";const auto unknown=parse(raw);
    require(unknown==decltype(unknown)::Unknown,"Unexpected token not Unknown");
    require(caption(unknown,raw)=="Unknown status (future_%s_token)","Unknown token lost or shown as failure");
    require(parse("")==decltype(unknown)::Unknown,"Empty token silently treated as idle");
}
inline void run() {
    const auto previous=i18n::state;i18n::choose(0);
    tokens(run_tokens,parse_run_state);tokens(record_tokens,parse_record_state);tokens(export_tokens,parse_export_state);tokens(source_metadata_tokens,parse_source_metadata_state);
    for(const auto& row:source_metadata_tokens)require(parse_record_state(row.token)==RecordState::Unknown,"Association token accepted as kept-record state");tokens(recovery_tokens,parse_recovery_state);
    for(const auto& row:recovery_tokens)require(parse_record_state(row.token)==RecordState::Unknown,"Association token accepted as kept-record state");
    check_caption(RunState::Idle);
    check_caption(RunState::Running);
    check_caption(RunState::Ready);
    check_caption(RunState::Stale);
    check_caption(RunState::Cancelling);
    check_caption(RunState::Cancelled);
    check_caption(RunState::Failed);
    check_caption(RunState::Verified);
    check_caption(RunState::Historical);
    check_caption(RunState::Unknown);
    check_caption(RecordState::Current);
    check_caption(RecordState::Unverified);
    check_caption(RecordState::ObjectChanged);
    check_caption(RecordState::SourceChanged);
    check_caption(RecordState::SourceClosed);
    check_caption(RecordState::VerificationFailed);
    check_caption(RecordState::Unknown);
    check_caption(ExportState::Idle);
    check_caption(ExportState::Writing);
    check_caption(ExportState::Succeeded);
    check_caption(ExportState::Cancelled);
    check_caption(ExportState::Failed);
    check_caption(ExportState::Unknown);
    check_caption(RunState::Kept);
    check_caption(RunState::Added);
    check_caption(RunState::Saved);
    check_caption(RunState::Partial);
    check_caption(RunState::Unresolved);
    check_caption(RunState::Unsupported);
    check_caption(RecordState::NoFeatures);
    check_caption(RecordState::GeometryUnavailable);
    check_caption(RecordState::GeometryChanged);
    check_caption(RecordState::ContextChanged);
    check_caption(RecordState::CalibrationChanged);
    check_caption(RecordState::ObjectsOrCalibrationChanged);
    check_caption(RecordState::ArchiveMissing);
    check_caption(RecordState::InvalidLabel);
    check_caption(RecordState::Removed);
    check_caption(ExportState::Cancelling);
    check_caption(ExportState::Stale);
    check_caption(SourceMetadataState::Unobserved);
    check_caption(SourceMetadataState::IncompatibleScheme);
    check_caption(SourceMetadataState::DifferentFile);
    check_caption(SourceMetadataState::VersionChanged);
    check_caption(SourceMetadataState::MatchingMetadata);
    check_caption(SourceMetadataState::MatchingPrimaryMetadata);
    check_caption(RecoveryState::Unsaved);
    check_caption(RecoveryState::Changed);
    check_caption(RecoveryState::Matching);
    check_caption(RecoveryState::Relocated);
    check_caption(RecoveryState::Unreadable);
    check_caption(RecoveryState::Missing);
    for(int locale=0;locale<i18n::language_count;++locale) {
        i18n::choose(locale);
        for(const auto& row:run_tokens)require(!caption(row.expected,row.token).empty(),"Empty localized caption");
        const auto rendered=caption(RunState::Unknown,"unexpected");
        require(rendered.find("unexpected")!=std::string::npos && rendered.find("%s")==std::string::npos,"Localized unknown token not rendered");
    }
    i18n::state=previous;std::cout<<"State vocabulary: exact inventory/parsers/captions/unknown/16 locales passed\n";
}
}
