#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/spatial_counts.hpp"
#include "lodepng.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>
#include <cstdlib>
using namespace qpx;namespace fs=std::filesystem;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<std::string> split(const std::string& s,char delimiter){std::vector<std::string> out;std::stringstream input(s);std::string field;while(std::getline(input,field,delimiter))out.push_back(field);return out;}
std::string read(const fs::path& path){std::ifstream f(path,std::ios::binary);require(bool(f),"Missing input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){try{
 require(argc==3,"Usage: prep RAW_DIRECTORY NEW_OUTPUT_DIRECTORY");const fs::path raw=fs::u8path(argv[1]),out=fs::u8path(argv[2]);require(fs::create_directory(out),"Output directory must be new");
 const auto factors=nlohmann::json::parse(read(raw/"scalefactors_json.json"));const double scale=factors.at("tissue_hires_scalef");require(std::isfinite(scale)&&scale>0,"Invalid published scale");
 auto png=read(raw/"tissue_hires_image.png");unsigned char* decoded=nullptr;unsigned width=0,height=0;const auto error=lodepng_decode32(&decoded,&width,&height,reinterpret_cast<const unsigned char*>(png.data()),png.size());require(!error,"PNG decode failed");std::unique_ptr<unsigned char,decltype(&std::free)> pixels(decoded,&std::free);require(width&&height&&std::uint64_t(width)*height<16000000,"Image size outside demo bound");
 std::vector<std::uint8_t> rgb(std::size_t(width)*height*3);for(std::size_t i=0;i<std::size_t(width)*height;++i){require(decoded[4*i+3]==255,"Image has nonopaque pixels");for(int c=0;c<3;++c)rgb[3*i+c]=decoded[4*i+c];}
 std::vector<std::string> barcodes;std::map<std::string,std::size_t> columns;std::string line;
 {std::ifstream f(raw/"barcodes.tsv");while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();require(!line.empty()&&columns.emplace(line,barcodes.size()).second,"Duplicate barcode");barcodes.push_back(line);}}require(barcodes.size()>0&&barcodes.size()<10000,"Unexpected spot count");
 std::vector<Point> positions(barcodes.size());std::set<std::size_t> matched;
 {std::ifstream f(raw/"tissue_positions_list.csv");while(std::getline(f,line)){auto v=split(line,',');require(v.size()==6,"Unexpected position schema");const auto at=columns.find(v[0]);if(at==columns.end())continue;require(v[1]=="1"&&matched.insert(at->second).second,"Filtered barcode not unique/in tissue");const double x=std::stod(v[5])*scale,y=std::stod(v[4])*scale;require(x>=0&&y>=0&&x<width&&y<height,"Scaled position outside image");positions[at->second]={x,y};}}require(matched.size()==barcodes.size(),"Missing position association");
 const std::set<std::string> desired{"Mbp","Plp1","Gad1","Slc17a7","Aqp4","Pcp4"};std::vector<std::string> features,symbols;std::map<std::uint64_t,std::size_t> rows;std::set<std::string> found;
 std::uint64_t total_features=0;{std::ifstream f(raw/"features.tsv");while(std::getline(f,line)){++total_features;auto v=split(line,'\t');require(v.size()==3,"Unexpected feature schema");if(!desired.count(v[1]))continue;require(found.insert(v[1]).second,"Ambiguous selected symbol");rows.emplace(total_features,features.size());features.push_back(v[1]+"|"+v[0]);symbols.push_back(v[1]);}}require(found==desired,"Missing selected marker");
 std::vector<std::uint64_t> values(barcodes.size()*features.size(),0);std::uint64_t source_entries=0;
 {std::ifstream f(raw/"matrix.mtx");std::getline(f,line);require(line=="%%MatrixMarket matrix coordinate integer general","Unexpected matrix encoding");do{require(bool(std::getline(f,line)),"Missing matrix dimensions");}while(line.empty()||line[0]=='%');std::uint64_t nr=0,nc=0;std::istringstream dims(line);dims>>nr>>nc>>source_entries;require(nr==total_features&&nc==barcodes.size()&&source_entries<100000000,"Matrix dimension mismatch");for(std::uint64_t i=0;i<source_entries;++i){std::uint64_t row,col,value;require(bool(f>>row>>col>>value),"Truncated matrix");require(row>0&&row<=nr&&col>0&&col<=nc,"Bad source index");const auto selected=rows.find(row);if(selected!=rows.end()){const auto at=std::size_t(col-1)*features.size()+selected->second;require(value<=9007199254740992ULL&&values[at]==0,"Invalid selected count/duplicate");values[at]=value;}}}
 std::uint64_t nnz=0;for(auto value:values)nnz+=value!=0;
 {std::ofstream f(out/"counts.mtx");f<<"%%MatrixMarket matrix coordinate integer general\n% Six original marker genes; no normalization\n"<<features.size()<<' '<<barcodes.size()<<' '<<nnz<<'\n';for(std::size_t c=0;c<barcodes.size();++c)for(std::size_t r=0;r<features.size();++r)if(values[c*features.size()+r])f<<r+1<<' '<<c+1<<' '<<values[c*features.size()+r]<<'\n';require(bool(f),"Count export failed");}
 {std::ofstream f(out/"features.tsv");f<<"feature_id\n";for(const auto& id:features)f<<id<<'\n';require(bool(f),"Feature export failed");}
 {std::ofstream f(out/"positions.tsv");f<<"observation_id\tx_px\ty_px\n"<<std::setprecision(17);for(std::size_t i=0;i<barcodes.size();++i)f<<barcodes[i]<<'\t'<<positions[i].x<<'\t'<<positions[i].y<<'\n';require(bool(f),"Position export failed");}
 Project project;auto document=std::make_shared<Document>(Image(width,height,std::move(rgb)));document->set_image_properties(std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"Dataset","10x Genomics V1_Adult_Mouse_Brain"},{"License","CC BY 4.0"},{"Coordinate conversion","full-resolution column/row multiplied by published tissue_hires_scalef"},{"Full-resolution spot diameter (px)",factors.at("spot_diameter_fullres").dump()}}));const auto image=project.add_document(document,"Real mouse brain H&E + spatial gene counts");project.save_new((out/"base.qpx").u8string());
 SpatialCountsOptions options;options.features=features;const auto report=import_spatial_counts((out/"base.qpx").u8string(),(out/"mouse-brain-spatial.qpx").u8string(),image,(out/"counts.mtx").u8string(),(out/"features.tsv").u8string(),(out/"positions.tsv").u8string(),options);
 nlohmann::json receipt={{"source_dataset","V1_Adult_Mouse_Brain"},{"source_url","https://www.10xgenomics.com/datasets/mouse-brain-section-coronal-1-standard-1-1-0"},{"license","CC BY 4.0"},{"source_features",total_features},{"source_entries",source_entries},{"spots",barcodes.size()},{"width",width},{"height",height},{"tissue_hires_scalef",scale},{"coordinate_transform","x=published_fullres_column*scale; y=published_fullres_row*scale"},{"selected_symbols",symbols},{"selected_feature_ids",features},{"selected_nonzero_counts",nnz},{"counts_modified",false},{"native_import",nlohmann::json::parse(report)}};
 std::ofstream(out/"preparation.json")<<receipt.dump(2)<<'\n';std::cout<<receipt.dump()<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
