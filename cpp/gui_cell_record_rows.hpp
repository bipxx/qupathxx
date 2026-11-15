#pragma once
#include "gui_record_list.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/cells.hpp"
namespace qpx::ui {
inline void cell_record_rows(const Document& document,RecordRows& cache) {
    cache.refresh(&document,document.revision(),document.cell_record_count(),[&](auto& rows){
        for(const auto& r:document.cell_records()) {
            std::string name;try{name=document.annotation(r->result->nuclei()->object_id()).name;}catch(const std::out_of_range&){}
            rows.push_back(record_row(r->id,name,"Cells",document.cell_status(r->id)));
        }
    },document.server().get());
}
}
