#include "qupathxx/expression_dataset.hpp"
#include "qupathxx/gene_display.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <thread>
using namespace qpx;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F action) {
    try {action();}catch(const E&){return;}throw std::runtime_error("Expected expression rejection");
}
ExpressionDatasetInput input() {
    ExpressionDatasetInput value;value.source_id="independent-count-fixture";value.coordinate_frame="image-level-zero-px";
    value.features={"A","B"};value.observations={{1,"s1",{1,2}},{2,"s2",{3,4}},{3,"s3",{5,6}},{4,"s4",{7,8}}};
    value.counts={0,7,2,9,10,11,99,13};value.valid={1,1,1,1,1,1,0,1};return value;
}
std::shared_ptr<const ObjectProperties> properties(const std::string& id,double value,char hash='a') {
    return std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},
        {"omics.matrix_sha256",std::string(64,hash)},{"omics.features_sha256",std::string(64,'b')},
        {"omics.positions_sha256",std::string(64,'c')},{"omics.observation_id",id}},
        std::map<std::string,double>{{"omics.count:A",value}});
}
void snapshot_tests() {
    auto original=input();const auto dataset=ExpressionDataset::create(original);
    original.counts[0]=123;check(dataset.counts()[0]==0 && dataset.counts()[6]==0,"Owned snapshot and canonical missing value");
    check(dataset.valid()[6]==0 && dataset.valid()[0]==1,"Measured zero differs from missing");
    auto changed=input();changed.counts[6]=888;
    check(ExpressionDataset::create(changed).sha256()==dataset.sha256(),"Invalid payload does not affect identity");
    changed.counts[0]=1;check(ExpressionDataset::create(changed).sha256()!=dataset.sha256(),"Counts affect identity");
    changed=input();changed.coordinate_frame="other-frame";check(ExpressionDataset::create(changed).sha256()!=dataset.sha256(),"Frame affects identity");
    changed=input();changed.counts[0]=std::numeric_limits<std::uint64_t>::max();
    check(ExpressionDataset::create(changed).counts()[0]==std::numeric_limits<std::uint64_t>::max(),"Full uint64 counts remain exact");
    changed=input();changed.features[1]="A";rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    changed=input();changed.observations[1].id="s1";rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    changed=input();changed.observations[1].object_id=1;rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    changed=input();changed.counts.pop_back();rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    changed=input();changed.valid[0]=2;rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    changed=input();changed.observations[0].position.x=std::numeric_limits<double>::infinity();rejects<std::invalid_argument>([&]{ExpressionDataset::create(changed);});
    rejects<WorkflowCaptureLimit>([&]{ExpressionDataset::create(input(),16);});
    changed=input();changed.counts.reserve(10000);rejects<WorkflowCaptureLimit>([&]{ExpressionDataset::create(std::move(changed),4096);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{ExpressionDataset::create(input(),65536,&cancelled);});
    auto continuous=input();continuous.domain=ExpressionValueDomain::Continuous;continuous.counts.clear();
    continuous.continuous={-3,0,0,1,8,2,std::numeric_limits<double>::quiet_NaN(),3};
    const auto signed_values=ExpressionDataset::create(continuous);check(signed_values.continuous()[0]==-3 && signed_values.continuous()[6]==0,"Signed continuous/missing domain");
    continuous.continuous[0]=std::numeric_limits<double>::infinity();rejects<std::invalid_argument>([&]{ExpressionDataset::create(continuous);});
    rejects<std::invalid_argument>([&]{gene_display_field(GeneDisplayRequest(signed_values,"A",[] {GeneDisplayOptions o;o.transform=GeneDisplayTransform::Log1p;return o;}()));});
    auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));
    const auto first=doc->add_detection(Roi::points({{1,2}}),"mutable display name");doc->set_object_properties(first,properties("stable-1",0));
    const auto second=doc->add_detection(Roi::points({{3,4}}));doc->set_object_properties(second,properties("stable-2",9007199254740992.));
    ExpressionCaptureOptions options;options.coordinate_frame="image-level-zero-px";options.features={"A","Absent"};
    const auto captured=ExpressionDataset::capture(*doc,options);
    check(captured.counts()==std::vector<std::uint64_t>({0,0,9007199254740992ULL,0}) && captured.valid()==std::vector<std::uint8_t>({1,0,1,0}),"Legacy projection exact boundary and missing feature");
    doc->rename_annotation(first,"renamed");check(ExpressionDataset::capture(*doc,options).sha256()==captured.sha256(),"Display name is not expression identity");
    doc->set_object_properties(first,properties("stable-1",2));check(ExpressionDataset::capture(*doc,options).sha256()!=captured.sha256(),"Recapture detects edited values");
    check(captured.counts()[0]==0,"Prior snapshot survives edits");
    doc->set_object_properties(first,properties("stable-1",9007199254740994.));rejects<std::invalid_argument>([&]{ExpressionDataset::capture(*doc,options);});
    doc->set_object_properties(first,properties("stable-1",2,'d'));rejects<std::invalid_argument>([&]{ExpressionDataset::capture(*doc,options);});
    options.source_id=captured.source_id();options.object_ids={second};check(ExpressionDataset::capture(*doc,options).observations().size()==1,"Explicit lineage/cohort selection");
    options.object_ids={first};rejects<std::invalid_argument>([&]{ExpressionDataset::capture(*doc,options);});
    options.object_ids={second,second};rejects<std::invalid_argument>([&]{ExpressionDataset::capture(*doc,options);});
    options.object_ids.clear();options.plane=ObjectPlane(0,0,0);rejects<std::invalid_argument>([&]{ExpressionDataset::capture(*doc,options);});
}
void display_tests() {
    const auto dataset=ExpressionDataset::create(input());const auto raw=dataset.counts();
    GeneDisplayOptions options;options.opacity=.5;
    const auto field=gene_display_field(GeneDisplayRequest(dataset,"A",options));
    check(field->legend.lower==0 && field->legend.upper==10 && field->legend.measured==3 && field->legend.missing==1 && field->legend.zeros==1,"Minmax legend");
    check(field->colors[0]==std::array<std::uint8_t,4>{20,20,24,128} && field->colors[3]==std::array<std::uint8_t,4>{145,145,145,128},"Zero/missing colors and alpha");
    check(field->colors[2]==std::array<std::uint8_t,4>{255,230,65,128},"Palette high endpoint");
    options.range=GeneDisplayRange::Percentile;options.lower_percentile=25;options.upper_percentile=75;
    const auto clipped=gene_display_field(GeneDisplayRequest(dataset,"A",options));
    check(clipped->legend.lower==1 && clipped->legend.upper==6 && clipped->legend.clipped_lower==1 && clipped->legend.clipped_upper==1,"Independent interpolated percentile and clipping");
    options.range=GeneDisplayRange::Manual;options.lower=2;options.upper=8;options.palette=GeneDisplayPalette::Gray;
    const auto manual=gene_display_field(GeneDisplayRequest(dataset,"A",options));
    check(manual->colors[1][0]==32 && manual->colors[2][0]==245 && manual->legend.clipped_lower==1 && manual->legend.clipped_upper==1,"Manual shared range and gray endpoints");
    options=GeneDisplayOptions{};options.transform=GeneDisplayTransform::Log1p;
    const auto log=gene_display_field(GeneDisplayRequest(dataset,"A",options));
    check(std::abs(log->transformed_values[1]-std::log(3.))<1e-15 && std::abs(log->legend.upper-std::log(11.))<1e-15,"Native log1p in display units");
    check(dataset.counts()==raw && log->dataset.counts()==raw,"Display preserves exact raw counts");
    const auto empty=gene_display_field(GeneDisplayRequest(dataset,"A",{},std::vector<std::size_t>{}));
    check(empty->rows.empty() && empty->legend.measured==0,"Explicit empty cohort stays empty");
    const auto subset=gene_display_field(GeneDisplayRequest(dataset,"A",{},std::vector<std::size_t>{2,0}));
    check(subset->transformed_values==std::vector<double>{10,0},"Cohort order preserved");
    rejects<std::invalid_argument>([&]{GeneDisplayRequest(dataset,"A",{},std::vector<std::size_t>{0,0});});
    rejects<std::invalid_argument>([&]{GeneDisplayRequest(dataset,"A",{},std::vector<std::size_t>{4});});
    rejects<std::invalid_argument>([&]{GeneDisplayRequest(dataset,"Unknown");});
    auto zero=input();std::fill(zero.counts.begin(),zero.counts.end(),0);
    const auto zeros=gene_display_field(GeneDisplayRequest(ExpressionDataset::create(zero),"A"));
    check(zeros->legend.lower==0 && zeros->legend.upper==0 && zeros->legend.zeros==3,"All-zero range");
    std::fill(zero.valid.begin(),zero.valid.end(),0);const auto missing=gene_display_field(GeneDisplayRequest(ExpressionDataset::create(zero),"A"));
    check(missing->legend.measured==0 && missing->legend.missing==4,"All-missing range");
    auto constant=input();constant.counts={5,5,5,5,5,5,5,5};constant.valid.clear();
    const auto fixed=gene_display_field(GeneDisplayRequest(ExpressionDataset::create(constant),"A"));
    check(fixed->legend.lower==5 && fixed->legend.upper==5 && fixed->colors[0]==fixed->colors[3],"Constant measured values use one color");
    auto extremes=input();extremes.domain=ExpressionValueDomain::Continuous;extremes.counts.clear();extremes.valid.clear();
    const auto largest=std::numeric_limits<double>::max();extremes.continuous={-largest,0,largest,1,1,2,-1,3};
    const auto wide=gene_display_field(GeneDisplayRequest(ExpressionDataset::create(extremes),"A"));
    check(wide->legend.lower==-largest && wide->legend.upper==largest && wide->colors[0][0]==24 && wide->colors[1][0]==255,"Signed extreme range does not overflow normalization");
    std::vector<std::size_t> retained{0};retained.reserve(10000);
    rejects<WorkflowCaptureLimit>([&]{GeneDisplayRequest(dataset,"A",{},std::move(retained),4096);});
    std::vector<std::size_t> capacity_and_work{0,1,2,3};capacity_and_work.reserve(490);
    rejects<WorkflowCaptureLimit>([&]{GeneDisplayRequest(dataset,"A",{},std::move(capacity_and_work),4096);});
    GeneDisplayCache cache(65536,1);GeneDisplayRequest a(dataset,"A"),b(dataset,"B");
    const auto hit=cache.get(a);check(cache.get(a)==hit && cache.entries()==1,"Identity cache hit");
    cache.get(b);check(cache.entries()==1 && cache.get(a)!=hit,"LRU count bound");
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{cache.get(a,&cancelled);});
    bool refused=false;std::thread worker([&]{try {cache.entries();}catch(const std::logic_error&){refused=true;}});worker.join();check(refused,"Controller cache owner enforced");
    GeneDisplayCache tiny(1,1);tiny.get(a);check(tiny.entries()==0 && tiny.retained_bytes()==0,"Oversized field returned without retention");
    auto changed=input();changed.counts[0]=1;check(GeneDisplayRequest(ExpressionDataset::create(changed),"A").sha256()!=a.sha256(),"Edited source cannot reuse display key");
    check(GeneDisplayRequest(dataset,"A",options).sha256()!=a.sha256(),"Changed transform changes key");
}
void persistence_test() {
    if(!has_project())return;
    auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));
    const auto id=doc->add_detection(Roi::points({{1,2}}));doc->set_object_properties(id,properties("persisted",9007199254740992.));
    ExpressionCaptureOptions options;options.coordinate_frame="image-level-zero-px";
    const auto before=ExpressionDataset::capture(*doc,options);
    gene_display_field(GeneDisplayRequest(before,"A"));
    Project project;project.add_document(doc);const auto file=std::filesystem::current_path()/(project.identity()+"-expression.qpx");
    project.save_new(file.u8string());const auto reopened=Project::open(file.u8string());
    const auto after=ExpressionDataset::capture(*reopened->document(1),options);
    check(after.sha256()==before.sha256() && after.counts()==before.counts(),"Save/reopen preserves raw counts and expression identity");
}
int main(int argc,char** argv) {try {
    snapshot_tests();display_tests();persistence_test();
    if(argc==2) {
        auto project=Project::open(argv[1]);auto doc=project->document(1);ExpressionCaptureOptions options;options.coordinate_frame="image-level-zero-px";
        const auto dataset=ExpressionDataset::capture(*doc,options);
        check(dataset.observations().size()==2702 && dataset.features().size()==6,"Admitted real demo dimensions");
        for(const auto& feature:dataset.features()) {
            const auto field=gene_display_field(GeneDisplayRequest(dataset,feature));
            check(field->legend.measured==2702 && !field->legend.missing,"Real marker display covers spots");
        }
        check(ExpressionDataset::capture(*doc,options).sha256()==dataset.sha256(),"Real project navigation preserves measurement identity");
    }
    std::cout<<"Expression snapshot and gene display: exact values/missingness/identity, projection capture, native ranges/colors/log1p, bounded cache and cancellation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
