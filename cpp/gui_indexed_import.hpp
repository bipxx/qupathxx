#pragma once
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/h5ad.hpp"
#include "gui_import_picker.hpp"
#include <cstring>
namespace qpx::detail {
// Thin file/catalogue controls over the existing native import worker.
class GuiIndexedImportInputs {
    enum Path {Matrix,Features,Observations,Store};
    std::array<std::array<char,32769>,4> paths_{};
    std::array<char,257> query_{};
    GuiImportPicker picker_;Path picking_=Store;
    int kind_=0,format_=1,domain_=0,unit_=0;std::uint64_t epoch_=0,submitted_=0;std::size_t page_=0;
    H5adOptions h5ad_;
    std::array<std::array<char,257>,5> interpretation_{};
    std::string generated_,expected_;ExpressionMatrixImportOptions options_;
    std::shared_ptr<const ExpressionMatrixSource> source_;
    std::shared_ptr<const ExpressionFeaturePage> page_result_;
    std::weak_ptr<const ExpressionMatrixSource> seen_source_;
    std::weak_ptr<const ExpressionFeaturePage> seen_page_;
    void copy(Path p,const std::string& value) {
        if(value.size()>32768)throw std::invalid_argument("Indexed input path exceeds32768 bytes");
        std::memcpy(paths_[p].data(),value.c_str(),value.size()+1);
    }
    void changed() {++epoch_;source_.reset();page_result_.reset();generated_.clear();expected_.clear();page_=0;}
    bool path(Path p,const char* label,std::vector<std::string> extensions,bool save=false) {
        ImGui::PushID(int(p));const bool edited=ui::InputText(label,paths_[p].data(),paths_[p].size());
        if(ui::Button("Browse...")){picking_=p;picker_.open(paths_[p].data(),std::move(extensions),save);}ImGui::PopID();return edited;
    }
public:
    GuiIndexedImportInputs(){std::strcpy(interpretation_[0].data(),"X");std::strcpy(interpretation_[1].data(),"obsm/spatial");}
    void set_store(const std::string& path,const std::string& expected={}) {copy(Store,path);kind_=0;changed();expected_=expected;}
    void set_index(const std::string& matrix,const std::string& features,const std::string& observations,
        const std::string& store,ExpressionMatrixImportOptions options) {
        copy(Matrix,matrix);copy(Features,features);copy(Observations,observations);copy(Store,store);
        kind_=1;options_=options;format_=int(options.format);changed();
    }
    void set_h5ad(const std::string& input,const std::string& store,const H5adOptions& options) {
        const std::array<std::string,5> fields{options.matrix,options.spatial,options.value_domain,options.frame.id,options.frame.source_id};
        for(const auto& field:fields)if(field.size()>256)throw std::invalid_argument("H5AD interpretation field exceeds256 bytes");
        copy(Matrix,input);copy(Store,store);h5ad_=options;kind_=2;
        for(std::size_t i=0;i<fields.size();++i)std::strcpy(interpretation_[i].data(),fields[i].c_str());
        domain_=options.domain?(*options.domain==ExpressionValueDomain::Counts?1:2):0;unit_=int(options.frame.unit)+1;changed();
    }
    void draw_frame()const {
        if(!source_ || !source_->metadata().spatial_frame){ui::TextWrapped("Read the indexed matrix to inspect its embedded source frame.");return;}
        const auto& m=source_->metadata();const auto& f=*m.spatial_frame;
        ui::TextWrapped("Embedded frame: %s | %s | %s",f.id.c_str(),f.source_id.c_str(),f.unit==CoordinateUnit::Pixel?"pixels":"micrometres");
        ui::TextWrapped("Domain: %s (%s). Matrix: %s. Spatial: %s.",m.domain==ExpressionValueDomain::Counts?"counts":"continuous",m.value_domain.c_str(),m.input_matrix.c_str(),m.input_spatial.c_str());
    }
    std::shared_ptr<const ExpressionMatrixSource> source()const{return source_;}
    const std::shared_ptr<const ExpressionFeaturePage>& catalogue()const{return page_result_;}
    std::string store()const{return source_?source_->path():std::string(paths_[Store].data());}
    void read(SpatialImportJob& job,std::size_t work,bool reload=false) {
        if(job.busy())return;submitted_=epoch_;options_.work_bytes=work;
        if(source_ && !reload){job.matrix_catalogue(source_,query_.data(),page_*24,24,work);return;}
        source_.reset();page_result_.reset();if(reload)expected_.clear();
        if(kind_ && generated_!=std::filesystem::absolute(std::filesystem::u8path(paths_[Store].data())).lexically_normal().u8string()) {
            if(kind_==2){
                if(!domain_ || !unit_)throw std::invalid_argument("Declare H5AD value domain and source coordinate units first");
                h5ad_.work_bytes=work;job.index_h5ad(paths_[Matrix].data(),paths_[Store].data(),h5ad_,options_);return;
            }
            options_.format=static_cast<ExpressionCatalogueFormat>(format_);
            job.index_matrix(paths_[Matrix].data(),paths_[Features].data(),paths_[Observations].data(),paths_[Store].data(),options_);
        } else job.open_matrix(paths_[Store].data(),expected_,work);
    }
    void poll(SpatialImportJob& job,std::size_t work) {
        if(const auto value=job.matrix_source();value && value!=seen_source_.lock()) {
            seen_source_=value;
            if(submitted_==epoch_) {
                source_=value;expected_=value->file_sha256();if(job.matrix_index_result())generated_=value->path();
                if(job.phase()==SpatialImportPhase::MatrixReady)read(job,work);
            }
        }
        if(const auto value=job.matrix_catalogue_result();value && value!=seen_page_.lock()) {
            seen_page_=value;if(submitted_==epoch_)page_result_=value;
        }
    }
    bool draw_interpretation() {
        if(kind_!=2)return false;
        bool edited=false;
            const char* labels[]={"Matrix key","Spatial key","Value domain label","Source frame ID","Source frame identity"};
            for(unsigned i=0;i<5;++i)edited=ui::InputText(labels[i],interpretation_[i].data(),interpretation_[i].size())||edited;
            edited=ui::Combo("Value interpretation",&domain_,"Choose explicitly\0Exact nonnegative counts\0Finite continuous values\0")||edited;
            edited=ui::Combo("Embedded coordinate units",&unit_,"Choose explicitly\0Pixels\0Micrometres\0")||edited;
            h5ad_.matrix=interpretation_[0].data();h5ad_.spatial=interpretation_[1].data();h5ad_.value_domain=interpretation_[2].data();
            h5ad_.frame.id=interpretation_[3].data();h5ad_.frame.source_id=interpretation_[4].data();
            h5ad_.domain=domain_?std::optional<ExpressionValueDomain>(domain_==1?ExpressionValueDomain::Counts:ExpressionValueDomain::Continuous):std::nullopt;
            if(unit_)h5ad_.frame.unit=static_cast<CoordinateUnit>(unit_-1);
            int z=int(h5ad_.frame.plane.z),t=int(h5ad_.frame.plane.t);
            if(ui::InputInt("Embedded source Z",&z)){if(z>=0 && z<65536)h5ad_.frame.plane.z=std::uint32_t(z);edited=true;}
            if(ui::InputInt("Embedded source T",&t)){if(t>=0 && t<65536)h5ad_.frame.plane.t=std::uint32_t(t);edited=true;}
        if(edited)changed();return edited;
    }
    bool draw_files() {
        ImGui::PushID("indexed-inputs");bool edited=false;
        edited=ui::Combo("Matrix input",&kind_,"Existing indexed matrix\0Index Matrix Market files\0Index H5AD\0");
        if(kind_==2){
            edited=path(Matrix,"H5AD file",{".h5ad"})||edited;
            edited=path(Store,"New indexed matrix (.qmx)",{".qmx"},true)||edited;
            ui::TextWrapped("Imports selected values, IDs and spatial coordinates. Other AnnData annotations, embeddings, graphs and raw are not round-tripped. X is never assumed to be raw counts.");
        }else if(kind_==1) {
            edited=path(Matrix,"Full counts (.mtx)",{".mtx"})||edited;
            edited=path(Features,"Feature axis (.tsv)",{".tsv"})||edited;
            edited=path(Observations,"Observation axis (.tsv)",{".tsv"})||edited;
            edited=ui::Combo("Axis profile",&format_,"Qpx: headered single-column IDs\0Tenx: headerless features / barcodes\0")||edited;
            edited=path(Store,"New indexed matrix (.qmx)",{".qmx"},true)||edited;
            ui::TextWrapped("Index once; retain all features and exact counts. Observation IDs are a separate axis from the coordinate table.");
        } else edited=path(Store,"Indexed matrix (.qmx)",{".qmx"})||edited;
        if(const auto selected=picker_.draw()){copy(picking_,*selected);edited=true;}
        if(edited)changed();ImGui::PopID();return edited;
    }
    void draw_catalogue(SpatialImportJob& job,std::size_t work) {
        if(ui::Button("Read / reload indexed matrix"))read(job,work,true);
        if(!source_)return;
        const auto& m=source_->metadata();
        ui::Text("%llu genes; %llu observations; all genes retained",static_cast<unsigned long long>(m.features),static_cast<unsigned long long>(m.observations));
        if(ui::InputText("Search full IDs / symbols",query_.data(),query_.size())){page_=0;read(job,work);}
        if(page_result_) {
            if(ImGui::BeginChild("indexed-catalogue",{0,std::min(ui::CollectionHeight(page_result_->features.size()),200.f*ui::design::scale())},ImGuiChildFlags_Borders))
                for(const auto& feature:page_result_->features)ui::TextWrapped("%s | %s",feature.id.c_str(),feature.name.c_str());
            ImGui::EndChild();
            ImGui::BeginDisabled(page_==0);if(ui::Button("Previous IDs")){--page_;read(job,work);}ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled((page_+1)*24>=page_result_->matches);if(ui::Button("Next IDs")){++page_;read(job,work);}ImGui::EndDisabled();
            ui::Text("%llu matching features; showing one page",static_cast<unsigned long long>(page_result_->matches));
        }
    }
};
}
