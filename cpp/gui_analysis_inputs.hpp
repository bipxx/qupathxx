#pragma once
#include "gui_workflow_ui.hpp"
#include <functional>

namespace qpx::detail {
inline ui::Availability analysis_inputs_availability(bool blocked) {return blocked?ui::WorkflowBlocked():ui::Availability{};}
struct AnalysisInputs {
    enum Field : unsigned {Kind=1,Vectors=2,White=4,Nucleus=8,IntensityStatistics=16,RoiSampling=32};
    int kind=0;std::vector<double> vectors,white;std::optional<std::array<ColorVector,3>> estimated_basis;
    ColorVector estimated_white{};bool nucleus=false,intensity_statistics=false;
    RoiSamplingPolicy roi_sampling=RoiSamplingPolicy::GeometryCoverage;std::uint64_t revision=0;
    unsigned changed_fields=0;
    std::function<void(unsigned)> apply_edit;
    explicit AnalysisInputs(const GuiOptions& o):kind(o.analysis_kind),vectors(o.analysis_vectors),white(o.analysis_white),
        nucleus(o.analysis_nucleus),intensity_statistics(o.analysis_intensity_statistics),roi_sampling(o.roi_sampling_policy) {}
    void edited(unsigned fields) {
        if(!fields)return;
        ++revision;changed_fields=fields;if(apply_edit)apply_edit(fields);
    }
    // A complete normalized basis affects the supplied-stain transform too,
    // even if its first two vectors happen to match the current editor values.
    void use_stains(const std::array<ColorVector,3>& basis,const ColorVector& reference) {
        unsigned fields=kind==1?0:Kind;
        // The cached reference selects the complete basis; every request's
        // actual white reference remains the editor's white value.
        if(estimated_basis!=std::optional<std::array<ColorVector,3>>(basis) || estimated_white!=reference)fields|=Vectors;
        for(std::size_t c=0;c<3;++c) {
            if(vectors[c]!=basis[0][c] || vectors[c+3]!=basis[1][c])fields|=Vectors;
            if(white[c]!=reference[c])fields|=White;
        }
        kind=1;estimated_basis=basis;estimated_white=reference;
        for(std::size_t c=0;c<3;++c){vectors[c]=basis[0][c];vectors[c+3]=basis[1][c];white[c]=reference[c];}
        edited(fields);
    }
};
inline constexpr unsigned region_inputs=AnalysisInputs::Kind|AnalysisInputs::Vectors|AnalysisInputs::White|AnalysisInputs::Nucleus|AnalysisInputs::IntensityStatistics|AnalysisInputs::RoiSampling;
inline constexpr unsigned tissue_inputs=AnalysisInputs::Vectors|AnalysisInputs::White|AnalysisInputs::Nucleus;
inline constexpr unsigned cell_inputs=AnalysisInputs::Vectors|AnalysisInputs::White;
inline constexpr unsigned training_inputs=AnalysisInputs::Vectors|AnalysisInputs::White;
inline constexpr unsigned stain_estimation_inputs=AnalysisInputs::White|AnalysisInputs::Nucleus;
enum class InputConsumer : std::size_t {Region,Tissue,Cells,Training,StainEstimation,Count};
struct AnalysisInputConsumer {unsigned fields;const char* cancelled;};
inline constexpr std::array<AnalysisInputConsumer,5> analysis_input_consumers={{
    {region_inputs,"Cancelled: region analysis (analysis inputs changed)."},
    {tissue_inputs,"Cancelled: tissue detection (analysis inputs changed)."},
    {cell_inputs,"Cancelled: cell detection (analysis inputs changed)."},
    {training_inputs,"Cancelled: pixel-classifier training (analysis inputs changed)."},
    {stain_estimation_inputs,"Cancelled: stain estimation (analysis inputs changed)."}
}};
struct AnalysisInputFeedback {
    struct Cancellation {bool active=false;std::uint64_t generation=0;};
    std::array<Cancellation,5> cancelled{};
    std::array<bool,5> last_edit{};
    enum class Editor {Region,Tissue,Cells,Profile,Model};
    Editor origin=Editor::Region,next_editor=Editor::Region;
    void begin(Editor editor){origin=editor;last_edit.fill(false);}
    void record(InputConsumer consumer,std::uint64_t generation) {
        const auto i=static_cast<std::size_t>(consumer);cancelled[i]={true,generation};last_edit[i]=true;
    }
    void observe(InputConsumer consumer,std::uint64_t generation) {
        auto& entry=cancelled[static_cast<std::size_t>(consumer)];if(entry.generation!=generation)entry.active=false;
    }
    std::string message(InputConsumer consumer,std::uint64_t generation) const {
        const auto i=static_cast<std::size_t>(consumer);const auto& entry=cancelled[i];
        return entry.active && entry.generation==generation?ui::i18n::text(analysis_input_consumers[i].cancelled):std::string{};
    }
    std::string report_message(InputConsumer consumer,std::uint64_t generation,const std::string& error) const {
        const auto cancelled=message(consumer,generation);
        if(error.empty())return cancelled;
        return cancelled.empty()?error:error+'\n'+cancelled;
    }
    void draw(Editor editor) const {
        const auto visible_here=[&](InputConsumer consumer) {
            if(editor==Editor::Tissue)return consumer==InputConsumer::Tissue;
            if(editor==Editor::Cells)return consumer==InputConsumer::Cells;
            return consumer==InputConsumer::Region || consumer==InputConsumer::StainEstimation;
        };
        if(origin==editor)for(std::size_t i=0;i<last_edit.size();++i)
            if(last_edit[i] && cancelled[i].active && !visible_here(static_cast<InputConsumer>(i)))ui::TextWrapped("%s",ui::i18n::text(analysis_input_consumers[i].cancelled));
    }
};
// Private replay observations keep the frozen public options/report layout.
struct AnalysisInputsTrace {
    struct Job {std::string state;std::uint64_t generation=0;bool busy=false,has_result=false;};
    std::array<Job,5> jobs;
    bool estimate_applied=false;
    bool training_trained=false;
    std::uint64_t active_image=0;
    std::uint64_t model_loads=0;
    std::optional<std::array<ColorVector,3>> tissue_request_stains;
    std::function<void(AnalysisInputs&)> before_frame;
    std::function<void(const AnalysisInputs&,const AnalysisInputFeedback&)> after_frame;
    std::function<void(const GuiReport&)> report;
    std::function<void()> after_render;
};
inline thread_local AnalysisInputsTrace* analysis_inputs_trace=nullptr;
// Only edited widgets compare their values. Idle frames allocate no snapshots
// and never rebuild the transform or notify consumers on behalf of this editor.
inline bool draw_analysis_inputs_editor(AnalysisInputs& inputs,unsigned fields) {
    unsigned changed=0;
    const auto scalar=[&](const char* label,int& value,const char* choices,unsigned field) {
        int next=value;if(ui::Combo(label,&next,choices) && next!=value){value=next;changed|=field;}
    };
    const auto flag=[&](const char* label,bool& value,unsigned field) {
        bool next=value;if(ui::Checkbox(label,&next) && next!=value){value=next;changed|=field;}
    };
    const auto vector=[&](const char* label,double* value,unsigned field) {
        std::array<double,3> next{value[0],value[1],value[2]};
        if(ui::InputScalarN(label,ImGuiDataType_Double,next.data(),3,nullptr,nullptr,"%.5g"))
            for(std::size_t c=0;c<3;++c)if(value[c]!=next[c]){value[c]=next[c];changed|=field;}
    };
    if(fields&AnalysisInputs::Kind)scalar("Features",inputs.kind,"RGB optical density\0Supplied stains\0",AnalysisInputs::Kind);
    if(fields&AnalysisInputs::Nucleus)flag("Use nucleus",inputs.nucleus,AnalysisInputs::Nucleus);
    if(fields&AnalysisInputs::IntensityStatistics)flag("Intensity statistics",inputs.intensity_statistics,AnalysisInputs::IntensityStatistics);
    if(fields&AnalysisInputs::RoiSampling) {
        int next=static_cast<int>(inputs.roi_sampling);const int old=next;
        if(ui::Combo("ROI polygon samples",&next,"Geometry coverage\0Polygon winding\0") && next!=old){inputs.roi_sampling=static_cast<RoiSamplingPolicy>(next);changed|=AnalysisInputs::RoiSampling;}
    }
    if(fields&AnalysisInputs::White)vector("White RGB",inputs.white.data(),AnalysisInputs::White);
    if(fields&AnalysisInputs::Vectors){vector("Stain 1",inputs.vectors.data(),AnalysisInputs::Vectors);vector("Stain 2",inputs.vectors.data()+3,AnalysisInputs::Vectors);}
    inputs.edited(changed);return changed!=0;
}
}
