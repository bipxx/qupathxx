// SPDX-License-Identifier: AGPL-3.0-only
// Public-only native consumer; the shared C++ services own all import behavior.
#include <qupathxx/format_import.hpp>
#include <qupathxx/project.hpp>
#include <charconv>
#include <filesystem>
#include <iostream>
int wmain(int argc,wchar_t** args) {
 try {
  if(argc<7 || (argc-7)%2)throw std::invalid_argument("MANIFEST PROVIDER PROJECT IMAGE_ID INPUT NEW_PROJECT [SOURCE_CLASS NATIVE_CLASS] [--plugin-options-json JSON]");
  auto text=[](const wchar_t* arg){return std::filesystem::path(arg).u8string();};
  const auto id_text=text(args[4]);std::uint64_t id=0;const auto parsed=std::from_chars(id_text.data(),id_text.data()+id_text.size(),id);
  if(!id || parsed.ec!=std::errc{} || parsed.ptr!=id_text.data()+id_text.size())throw std::invalid_argument("Invalid image ID");
  qpx::configure_format_plugins(text(args[1]));auto project=qpx::Project::open_without_journal_recovery(text(args[3]));auto document=project->document(id);
  qpx::AnnotationPluginImportOptions options;std::optional<std::string> plugin_options;bool mapped=false;
  for(int at=7;at<argc;at+=2) {
   const auto key=text(args[at]);
   if(key=="--plugin-options-json") {
    if(plugin_options)throw std::invalid_argument("Duplicate plugin options");
    plugin_options=text(args[at+1]);
   } else {
    if(mapped || key.rfind("--",0)==0)throw std::invalid_argument("Invalid class mapping or unknown option");
    options.class_map.emplace(key,text(args[at+1]));mapped=true;
   }
  }
  auto preview=qpx::preview_annotation_plugin(*document,text(args[2]),text(args[5]),options,plugin_options);const auto result=qpx::apply_annotation_plugin(*document,preview);project->save_new(text(args[6]));
  std::cout<<result.preview_sha256<<'\n'<<qpx::format_plugin_receipt_json(result.receipt)<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
