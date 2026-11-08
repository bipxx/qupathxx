#include "qupathxx/gene_view.hpp"
#include "qupathxx/workflow.hpp"
#include <iostream>
#include <thread>
using namespace qpx;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F action) {try {action();}catch(const E&){return;}throw std::runtime_error("Expected gene-view refusal");}
ExpressionDatasetInput input() {
    ExpressionDatasetInput value;value.source_id="gene-view-fixture";value.coordinate_frame="image-level-zero-px";
    value.features={"Gad1|ENS1","Mbp|ENS2"};value.observations={{1,"s1",{1,2}},{2,"s2",{3,4}},{3,"s3",{5,6}},{4,"s4",{7,8}}};
    value.counts={0,100,2,200,10,300,999,400};value.valid={1,1,1,1,1,1,0,1};return value;
}
void pair_tests() {
    const auto dataset=ExpressionDataset::create(input());GeneDisplayOptions options;
    const std::array<std::string,2> genes{dataset.features()[0],dataset.features()[1]};
    const auto pair=gene_display_pair(dataset,genes,options,true,true);
    check(pair.fields[0]->legend.lower==0 && pair.fields[0]->legend.upper==400 && pair.fields[1]->legend.upper==400,"Shared minmax uses both genes");
    options.range=GeneDisplayRange::Percentile;options.lower_percentile=25;options.upper_percentile=75;
    const auto percentile=gene_display_pair(dataset,genes,options,true,true);
    check(percentile.fields[0]->legend.lower==6 && percentile.fields[0]->legend.upper==250 && percentile.fields[1]->legend.lower==6,"Pooled percentile oracle [0,2,10,100,200,300,400]");
    check(percentile.range_policy==GeneDisplayRange::Percentile && percentile.shared_range,"Comparison retains requested shared range policy");
    const auto separate=gene_display_pair(dataset,genes,{},true,false);
    check(separate.fields[0]->legend.upper==10 && separate.fields[1]->legend.lower==100,"Independent scales remain explicit");
    const auto single=gene_display_pair(dataset,genes,{},false,true);check(single.fields[0] && !single.fields[1] && !single.shared_range,"Single-gene view does not compute another gene");
    auto missing=input();missing.valid={0,1,0,1,0,1,0,1};missing.counts={0,5,0,5,0,5,0,5};
    const auto constant=gene_display_pair(ExpressionDataset::create(missing),genes,{},true,true);
    check(constant.fields[0]->legend.lower==5 && constant.fields[1]->legend.upper==5 && constant.fields[0]->legend.missing==4,"Missing gene does not invent zero in shared constant range");
    std::fill(missing.valid.begin(),missing.valid.end(),0);const auto absent=gene_display_pair(ExpressionDataset::create(missing),genes,{},true,true);
    check(absent.fields[0]->legend.lower==0 && absent.fields[1]->legend.upper==0 && absent.fields[1]->legend.missing==4,"All-missing comparison");
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{gene_display_pair(dataset,genes,{},true,true,65536,&cancelled);});
    rejects<WorkflowCaptureLimit>([&]{gene_display_pair(dataset,genes,{},true,true,128);});
    check(dataset.counts()==ExpressionDataset::create(input()).counts(),"Comparison never changes raw counts");
}
void session_tests() {
    const auto dataset=ExpressionDataset::create(input());GeneViewSession session;session.bind(dataset);
    auto options=session.options();options.enabled=true;options.compare=true;session.configure(options);session.poll();
    options.genes={dataset.features()[1],dataset.features()[0]};options.display.transform=GeneDisplayTransform::Log1p;
    session.configure(options);check(!session.result(),"Rapid switch clears old ready field immediately");session.poll();
    options.display.transform=GeneDisplayTransform::Linear;session.configure(options);session.wait();
    const auto ready=session.result();check(ready && ready->fields[0]->feature==dataset.features()[1] && session.error().empty(),"Only latest coalesced request publishes");
    check(ready->fields[0]->legend.transform==GeneDisplayTransform::Linear,"Superseded log request cannot publish");
    const auto generation=session.generation();session.bind(dataset);session.configure(options);check(session.generation()==generation,"Unchanged snapshot/options do not rerun work");
    auto changed=input();changed.counts[0]=99;const auto updated=ExpressionDataset::create(changed);session.bind(updated);
    check(!session.result(),"Edited dataset invalidates ready colors");session.wait();check(session.result()->fields[0]->dataset.sha256()==updated.sha256(),"Dataset identity retained in new display");
    check(ready->fields[0]->dataset.sha256()==dataset.sha256(),"Old result ownership survives replacement");
    session.refresh();session.poll();session.cancel();session.wait();check(!session.result() && !session.busy(),"Cancellation cannot publish superseded colors");
    session.refresh();session.wait();check(bool(session.result()),"Explicit refresh resumes display");
    bool refused=false;std::thread wrong_owner([&]{try {session.options();}catch(const std::logic_error&){refused=true;}});wrong_owner.join();check(refused,"Controller owner thread enforced");
    session.clear();session.wait();check(!session.dataset() && !session.result(),"Leaving document clears current expression");
}
void catalogue_and_preferences() {
    auto values=input();values.features.clear();values.observations.resize(1);values.valid.clear();values.counts.assign(80,0);
    for(unsigned i=0;i<80;++i)values.features.push_back("Gene"+std::to_string(i)+"|ENS"+std::to_string(i));
    const auto dataset=ExpressionDataset::create(values);const auto all=gene_feature_page(dataset,"",2,16);
    check(all.matches==80 && all.indices.size()==16 && all.indices[0]==32,"Paged catalogue retains stable feature indices");
    const auto id=gene_feature_page(dataset,"eNs73");check(id.matches==1 && id.indices[0]==73,"Search both symbols and IDs with ASCII case folding");
    check(gene_feature_page(dataset,"absent").indices.empty(),"Empty search is explicit");rejects<std::invalid_argument>([&]{gene_feature_page(dataset,"",0,0);});
    GeneViewOptions options;options.enabled=true;options.compare=true;options.shared_range=false;options.genes={"Gad1|ENS1","Mbp|ENS2"};
    options.display.transform=GeneDisplayTransform::Log1p;options.display.range=GeneDisplayRange::Manual;options.display.lower=0;options.display.upper=3.141592653589793;
    options.display.palette=GeneDisplayPalette::Gray;options.display.opacity=.375;options.radius=7.25;options.display.zero_color={1,2,3,4};options.display.missing_color={5,6,7,8};
    const auto text=gene_view_preference(options);check(gene_view_preference(parse_gene_view_preference(text))==text,"Exact native presentation preference round trip");
    check(gene_view_preference(parse_gene_view_preference(gene_view_preference({})))==gene_view_preference({}),"Empty initial gene choices round trip");
    const auto workspace=gene_workspace_preference(options,std::string(64,'a'));const auto restored=parse_gene_workspace_preference(workspace);
    check(restored.source_id==std::string(64,'a') && gene_workspace_preference(restored.view,restored.source_id)==workspace,"Workspace retains explicit source choice");
    check(parse_gene_workspace_preference(text).source_id.empty(),"Existing presentation-only preference remains readable");
    rejects<std::invalid_argument>([&]{gene_workspace_preference(options,"unknown-source");});
    rejects<std::invalid_argument>([&]{parse_gene_view_preference(text+"\n");});rejects<std::invalid_argument>([]{parse_gene_view_preference("qpx.gene-view/2");});
    options.display.opacity=2;rejects<std::invalid_argument>([&]{gene_view_preference(options);});
}
int main(){try {pair_tests();session_tests();catalogue_and_preferences();std::cout<<"Gene view: pooled scales, immutable latest-generation jobs, search/pages, cancellation and native workspace preferences passed\n";}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
