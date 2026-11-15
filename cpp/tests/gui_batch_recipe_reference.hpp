#pragma once
// Independent, frozen pre-task Run assembly and exhaustive member comparisons.
namespace batch_recipe_checks {
using namespace qpx;
template<class T>struct Compare {static void run(const T& a,const T& b,const std::string& field){check(a==b,"Batch option differs: "+field);}};
template<class T>struct Compare<std::optional<T>> {template<class A,class B>static void run(const A& a,const B& b,const std::string& f){check(bool(a)==bool(b),"Batch optional differs: "+f);if(a)Compare<T>::run(*a,*b,f);}};
template<class T>struct Compare<std::shared_ptr<const T>> {template<class A,class B>static void run(const A& a,const B& b,const std::string& f){check(bool(a)==bool(b),"Batch pointer differs: "+f);if(a)Compare<T>::run(*a,*b,f);}};
template<>struct Compare<PixelClassifier>{template<class A,class B>static void run(const A& a,const B& b,const std::string& f){check(a.bytes()==b.bytes(),"Batch pixel model differs: "+f);}};
template<>struct Compare<RawPixelClassifier>{template<class A,class B>static void run(const A& a,const B& b,const std::string& f){check(a.bytes()==b.bytes(),"Batch raw model differs: "+f);}};
template<>struct Compare<ScientificGridAxis>{template<class A,class B>static void run(const A& a,const B& b,const std::string& f){check(std::make_tuple(a.origin(),a.extent(),a.samples(),a.context())==std::make_tuple(b.origin(),b.extent(),b.samples(),b.context()),"Batch axis differs: "+f);}};
template<>struct Compare<TissueContourOptions> {static void run(const TissueContourOptions& a,const TissueContourOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.clip_to_roi)>>::run(a.clip_to_roi,b.clip_to_roi,f+".clip_to_roi");
    Compare<std::decay_t<decltype(a.max_components)>>::run(a.max_components,b.max_components,f+".max_components");
    Compare<std::decay_t<decltype(a.max_edges)>>::run(a.max_edges,b.max_edges,f+".max_edges");
    Compare<std::decay_t<decltype(a.max_vertices)>>::run(a.max_vertices,b.max_vertices,f+".max_vertices");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
}};
template<>struct Compare<TissueParameters> {static void run(const TissueParameters& a,const TissueParameters& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.feature)>>::run(a.feature,b.feature,f+".feature");
    Compare<std::decay_t<decltype(a.threshold)>>::run(a.threshold,b.threshold,f+".threshold");
    Compare<std::decay_t<decltype(a.connectivity)>>::run(a.connectivity,b.connectivity,f+".connectivity");
    Compare<std::decay_t<decltype(a.level)>>::run(a.level,b.level,f+".level");
    Compare<std::decay_t<decltype(a.open_radius)>>::run(a.open_radius,b.open_radius,f+".open_radius");
    Compare<std::decay_t<decltype(a.close_radius)>>::run(a.close_radius,b.close_radius,f+".close_radius");
    Compare<std::decay_t<decltype(a.fill_holes)>>::run(a.fill_holes,b.fill_holes,f+".fill_holes");
    Compare<std::decay_t<decltype(a.max_hole_pixels)>>::run(a.max_hole_pixels,b.max_hole_pixels,f+".max_hole_pixels");
    Compare<std::decay_t<decltype(a.min_pixels)>>::run(a.min_pixels,b.min_pixels,f+".min_pixels");
    Compare<std::decay_t<decltype(a.max_pixels)>>::run(a.max_pixels,b.max_pixels,f+".max_pixels");
    Compare<std::decay_t<decltype(a.tile_size)>>::run(a.tile_size,b.tile_size,f+".tile_size");
    Compare<std::decay_t<decltype(a.max_components)>>::run(a.max_components,b.max_components,f+".max_components");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
    Compare<std::decay_t<decltype(a.sampling_policy)>>::run(a.sampling_policy,b.sampling_policy,f+".sampling_policy");
    Compare<std::decay_t<decltype(a.smoothing_sigma)>>::run(a.smoothing_sigma,b.smoothing_sigma,f+".smoothing_sigma");
    Compare<std::decay_t<decltype(a.median_cleanup)>>::run(a.median_cleanup,b.median_cleanup,f+".median_cleanup");
    Compare<std::decay_t<decltype(a.canonical_pyramid)>>::run(a.canonical_pyramid,b.canonical_pyramid,f+".canonical_pyramid");
}};
template<>struct Compare<CellParameters> {static void run(const CellParameters& a,const CellParameters& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.algorithm)>>::run(a.algorithm,b.algorithm,f+".algorithm");
    Compare<std::decay_t<decltype(a.log_prominence)>>::run(a.log_prominence,b.log_prominence,f+".log_prominence");
    Compare<std::decay_t<decltype(a.context_padding)>>::run(a.context_padding,b.context_padding,f+".context_padding");
    Compare<std::decay_t<decltype(a.feature)>>::run(a.feature,b.feature,f+".feature");
    Compare<std::decay_t<decltype(a.threshold)>>::run(a.threshold,b.threshold,f+".threshold");
    Compare<std::decay_t<decltype(a.sigma)>>::run(a.sigma,b.sigma,f+".sigma");
    Compare<std::decay_t<decltype(a.min_peak_radius)>>::run(a.min_peak_radius,b.min_peak_radius,f+".min_peak_radius");
    Compare<std::decay_t<decltype(a.min_seed_distance)>>::run(a.min_seed_distance,b.min_seed_distance,f+".min_seed_distance");
    Compare<std::decay_t<decltype(a.cell_expansion)>>::run(a.cell_expansion,b.cell_expansion,f+".cell_expansion");
    Compare<std::decay_t<decltype(a.min_nucleus_pixels)>>::run(a.min_nucleus_pixels,b.min_nucleus_pixels,f+".min_nucleus_pixels");
    Compare<std::decay_t<decltype(a.max_nucleus_pixels)>>::run(a.max_nucleus_pixels,b.max_nucleus_pixels,f+".max_nucleus_pixels");
    Compare<std::decay_t<decltype(a.connectivity)>>::run(a.connectivity,b.connectivity,f+".connectivity");
    Compare<std::decay_t<decltype(a.level)>>::run(a.level,b.level,f+".level");
    Compare<std::decay_t<decltype(a.tile_size)>>::run(a.tile_size,b.tile_size,f+".tile_size");
    Compare<std::decay_t<decltype(a.max_cells)>>::run(a.max_cells,b.max_cells,f+".max_cells");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
    Compare<std::decay_t<decltype(a.contours)>>::run(a.contours,b.contours,f+".contours");
}};
template<>struct Compare<GridCellParameters> {static void run(const GridCellParameters& a,const GridCellParameters& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.processing)>>::run(a.processing,b.processing,f+".processing");
    Compare<std::decay_t<decltype(a.units)>>::run(a.units,b.units,f+".units");
    Compare<std::decay_t<decltype(a.feature)>>::run(a.feature,b.feature,f+".feature");
    Compare<std::decay_t<decltype(a.threshold)>>::run(a.threshold,b.threshold,f+".threshold");
    Compare<std::decay_t<decltype(a.sigma)>>::run(a.sigma,b.sigma,f+".sigma");
    Compare<std::decay_t<decltype(a.log_prominence)>>::run(a.log_prominence,b.log_prominence,f+".log_prominence");
    Compare<std::decay_t<decltype(a.min_peak_radius)>>::run(a.min_peak_radius,b.min_peak_radius,f+".min_peak_radius");
    Compare<std::decay_t<decltype(a.min_seed_distance)>>::run(a.min_seed_distance,b.min_seed_distance,f+".min_seed_distance");
    Compare<std::decay_t<decltype(a.cell_expansion)>>::run(a.cell_expansion,b.cell_expansion,f+".cell_expansion");
    Compare<std::decay_t<decltype(a.min_nucleus_area)>>::run(a.min_nucleus_area,b.min_nucleus_area,f+".min_nucleus_area");
    Compare<std::decay_t<decltype(a.max_nucleus_area)>>::run(a.max_nucleus_area,b.max_nucleus_area,f+".max_nucleus_area");
    Compare<std::decay_t<decltype(a.sampling_policy)>>::run(a.sampling_policy,b.sampling_policy,f+".sampling_policy");
    Compare<std::decay_t<decltype(a.max_cells)>>::run(a.max_cells,b.max_cells,f+".max_cells");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
    Compare<std::decay_t<decltype(a.contours)>>::run(a.contours,b.contours,f+".contours");
}};
template<>struct Compare<GridCellSampling> {static void run(const GridCellSampling& a,const GridCellSampling& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.samples_x)>>::run(a.samples_x,b.samples_x,f+".samples_x");
    Compare<std::decay_t<decltype(a.samples_y)>>::run(a.samples_y,b.samples_y,f+".samples_y");
    Compare<std::decay_t<decltype(a.context_x)>>::run(a.context_x,b.context_x,f+".context_x");
    Compare<std::decay_t<decltype(a.context_y)>>::run(a.context_y,b.context_y,f+".context_y");
}};
template<>struct Compare<TissueGridSampling> {static void run(const TissueGridSampling& a,const TissueGridSampling& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.samples_x)>>::run(a.samples_x,b.samples_x,f+".samples_x");
    Compare<std::decay_t<decltype(a.samples_y)>>::run(a.samples_y,b.samples_y,f+".samples_y");
    Compare<std::decay_t<decltype(a.context_x)>>::run(a.context_x,b.context_x,f+".context_x");
    Compare<std::decay_t<decltype(a.context_y)>>::run(a.context_y,b.context_y,f+".context_y");
}};
template<>struct Compare<RawGridSampling> {static void run(const RawGridSampling& a,const RawGridSampling& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.samples_x)>>::run(a.samples_x,b.samples_x,f+".samples_x");
    Compare<std::decay_t<decltype(a.samples_y)>>::run(a.samples_y,b.samples_y,f+".samples_y");
    Compare<std::decay_t<decltype(a.context_x)>>::run(a.context_x,b.context_x,f+".context_x");
    Compare<std::decay_t<decltype(a.context_y)>>::run(a.context_y,b.context_y,f+".context_y");
}};
template<>struct Compare<NominalCellRoiSampling> {static void run(const NominalCellRoiSampling& a,const NominalCellRoiSampling& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.downsample)>>::run(a.downsample,b.downsample,f+".downsample");
    Compare<std::decay_t<decltype(a.context_x)>>::run(a.context_x,b.context_x,f+".context_x");
    Compare<std::decay_t<decltype(a.context_y)>>::run(a.context_y,b.context_y,f+".context_y");
    Compare<std::decay_t<decltype(a.sampling)>>::run(a.sampling,b.sampling,f+".sampling");
}};
template<>struct Compare<PixelPredictionOptions> {static void run(const PixelPredictionOptions& a,const PixelPredictionOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.tile_size)>>::run(a.tile_size,b.tile_size,f+".tile_size");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
    Compare<std::decay_t<decltype(a.min_vote_fraction)>>::run(a.min_vote_fraction,b.min_vote_fraction,f+".min_vote_fraction");
}};
template<>struct Compare<PixelStreamOptions> {static void run(const PixelStreamOptions& a,const PixelStreamOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.prediction)>>::run(a.prediction,b.prediction,f+".prediction");
    Compare<std::decay_t<decltype(a.block_size)>>::run(a.block_size,b.block_size,f+".block_size");
    Compare<std::decay_t<decltype(a.max_pixels)>>::run(a.max_pixels,b.max_pixels,f+".max_pixels");
    Compare<std::decay_t<decltype(a.max_blocks)>>::run(a.max_blocks,b.max_blocks,f+".max_blocks");
}};
template<>struct Compare<RawPixelStreamOptions> {static void run(const RawPixelStreamOptions& a,const RawPixelStreamOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.tile_width)>>::run(a.tile_width,b.tile_width,f+".tile_width");
    Compare<std::decay_t<decltype(a.tile_height)>>::run(a.tile_height,b.tile_height,f+".tile_height");
    Compare<std::decay_t<decltype(a.max_samples)>>::run(a.max_samples,b.max_samples,f+".max_samples");
    Compare<std::decay_t<decltype(a.max_windows)>>::run(a.max_windows,b.max_windows,f+".max_windows");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
    Compare<std::decay_t<decltype(a.min_vote_fraction)>>::run(a.min_vote_fraction,b.min_vote_fraction,f+".min_vote_fraction");
}};
template<>struct Compare<RawPixelStreamContourOptions> {static void run(const RawPixelStreamContourOptions& a,const RawPixelStreamContourOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.contours)>>::run(a.contours,b.contours,f+".contours");
    Compare<std::decay_t<decltype(a.max_boundary_runs)>>::run(a.max_boundary_runs,b.max_boundary_runs,f+".max_boundary_runs");
    Compare<std::decay_t<decltype(a.boundary_work_bytes)>>::run(a.boundary_work_bytes,b.boundary_work_bytes,f+".boundary_work_bytes");
    Compare<std::decay_t<decltype(a.work_bytes)>>::run(a.work_bytes,b.work_bytes,f+".work_bytes");
}};
template<>struct Compare<RegionRequest> {static void run(const RegionRequest& a,const RegionRequest& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.x)>>::run(a.x,b.x,f+".x");
    Compare<std::decay_t<decltype(a.y)>>::run(a.y,b.y,f+".y");
    Compare<std::decay_t<decltype(a.level)>>::run(a.level,b.level,f+".level");
    Compare<std::decay_t<decltype(a.width)>>::run(a.width,b.width,f+".width");
    Compare<std::decay_t<decltype(a.height)>>::run(a.height,b.height,f+".height");
}};
template<>struct Compare<RawRegionRequest> {static void run(const RawRegionRequest& a,const RawRegionRequest& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.region)>>::run(a.region,b.region,f+".region");
    Compare<std::decay_t<decltype(a.first_channel)>>::run(a.first_channel,b.first_channel,f+".first_channel");
    Compare<std::decay_t<decltype(a.channels)>>::run(a.channels,b.channels,f+".channels");
    Compare<std::decay_t<decltype(a.z)>>::run(a.z,b.z,f+".z");
    Compare<std::decay_t<decltype(a.t)>>::run(a.t,b.t,f+".t");
}};
template<>struct Compare<ProjectBatchOptions> {static void run(const ProjectBatchOptions& a,const ProjectBatchOptions& b,const std::string& f) {
    Compare<std::decay_t<decltype(a.kind)>>::run(a.kind,b.kind,f+".kind");
    Compare<std::decay_t<decltype(a.failure)>>::run(a.failure,b.failure,f+".failure");
    Compare<std::decay_t<decltype(a.roi_kind)>>::run(a.roi_kind,b.roi_kind,f+".roi_kind");
    Compare<std::decay_t<decltype(a.object_kind)>>::run(a.object_kind,b.object_kind,f+".object_kind");
    Compare<std::decay_t<decltype(a.nucleus)>>::run(a.nucleus,b.nucleus,f+".nucleus");
    Compare<std::decay_t<decltype(a.resume)>>::run(a.resume,b.resume,f+".resume");
    Compare<std::decay_t<decltype(a.publish_cells)>>::run(a.publish_cells,b.publish_cells,f+".publish_cells");
    Compare<std::decay_t<decltype(a.tma_only)>>::run(a.tma_only,b.tma_only,f+".tma_only");
    Compare<std::decay_t<decltype(a.include_missing_tma)>>::run(a.include_missing_tma,b.include_missing_tma,f+".include_missing_tma");
    Compare<std::decay_t<decltype(a.roi_intensity_statistics)>>::run(a.roi_intensity_statistics,b.roi_intensity_statistics,f+".roi_intensity_statistics");
    Compare<std::decay_t<decltype(a.roi_tile_size)>>::run(a.roi_tile_size,b.roi_tile_size,f+".roi_tile_size");
    Compare<std::decay_t<decltype(a.snapshot_budget)>>::run(a.snapshot_budget,b.snapshot_budget,f+".snapshot_budget");
    Compare<std::decay_t<decltype(a.materialize_budget)>>::run(a.materialize_budget,b.materialize_budget,f+".materialize_budget");
    Compare<std::decay_t<decltype(a.result_budget)>>::run(a.result_budget,b.result_budget,f+".result_budget");
    Compare<std::decay_t<decltype(a.csv_limit)>>::run(a.csv_limit,b.csv_limit,f+".csv_limit");
    Compare<std::decay_t<decltype(a.checkpoint_limit)>>::run(a.checkpoint_limit,b.checkpoint_limit,f+".checkpoint_limit");
    Compare<std::decay_t<decltype(a.tissue)>>::run(a.tissue,b.tissue,f+".tissue");
    Compare<std::decay_t<decltype(a.cells)>>::run(a.cells,b.cells,f+".cells");
    Compare<std::decay_t<decltype(a.cell_grid)>>::run(a.cell_grid,b.cell_grid,f+".cell_grid");
    Compare<std::decay_t<decltype(a.cell_nominal)>>::run(a.cell_nominal,b.cell_nominal,f+".cell_nominal");
    Compare<std::decay_t<decltype(a.grid_cells)>>::run(a.grid_cells,b.grid_cells,f+".grid_cells");
    Compare<std::decay_t<decltype(a.tissue_min_component_area_um2)>>::run(a.tissue_min_component_area_um2,b.tissue_min_component_area_um2,f+".tissue_min_component_area_um2");
    Compare<std::decay_t<decltype(a.tissue_max_component_area_um2)>>::run(a.tissue_max_component_area_um2,b.tissue_max_component_area_um2,f+".tissue_max_component_area_um2");
    Compare<std::decay_t<decltype(a.cell_min_nucleus_area_um2)>>::run(a.cell_min_nucleus_area_um2,b.cell_min_nucleus_area_um2,f+".cell_min_nucleus_area_um2");
    Compare<std::decay_t<decltype(a.cell_max_nucleus_area_um2)>>::run(a.cell_max_nucleus_area_um2,b.cell_max_nucleus_area_um2,f+".cell_max_nucleus_area_um2");
    Compare<std::decay_t<decltype(a.pixel_model)>>::run(a.pixel_model,b.pixel_model,f+".pixel_model");
    Compare<std::decay_t<decltype(a.pixels)>>::run(a.pixels,b.pixels,f+".pixels");
    Compare<std::decay_t<decltype(a.pixel_file_limit)>>::run(a.pixel_file_limit,b.pixel_file_limit,f+".pixel_file_limit");
    Compare<std::decay_t<decltype(a.pixel_total_limit)>>::run(a.pixel_total_limit,b.pixel_total_limit,f+".pixel_total_limit");
    Compare<std::decay_t<decltype(a.raw_pixel_model)>>::run(a.raw_pixel_model,b.raw_pixel_model,f+".raw_pixel_model");
    Compare<std::decay_t<decltype(a.raw_pixel_x)>>::run(a.raw_pixel_x,b.raw_pixel_x,f+".raw_pixel_x");
    Compare<std::decay_t<decltype(a.raw_pixel_y)>>::run(a.raw_pixel_y,b.raw_pixel_y,f+".raw_pixel_y");
    Compare<std::decay_t<decltype(a.raw_pixels)>>::run(a.raw_pixels,b.raw_pixels,f+".raw_pixels");
    Compare<std::decay_t<decltype(a.raw_contours)>>::run(a.raw_contours,b.raw_contours,f+".raw_contours");
    Compare<std::decay_t<decltype(a.raw_contour_prefix)>>::run(a.raw_contour_prefix,b.raw_contour_prefix,f+".raw_contour_prefix");
    Compare<std::decay_t<decltype(a.raw_contour_publication_bytes)>>::run(a.raw_contour_publication_bytes,b.raw_contour_publication_bytes,f+".raw_contour_publication_bytes");
    Compare<std::decay_t<decltype(a.raw_pixel_file_work_bytes)>>::run(a.raw_pixel_file_work_bytes,b.raw_pixel_file_work_bytes,f+".raw_pixel_file_work_bytes");
    Compare<std::decay_t<decltype(a.raw_grid)>>::run(a.raw_grid,b.raw_grid,f+".raw_grid");
    Compare<std::decay_t<decltype(a.raw_grid_work_bytes)>>::run(a.raw_grid_work_bytes,b.raw_grid_work_bytes,f+".raw_grid_work_bytes");
    Compare<std::decay_t<decltype(a.raw)>>::run(a.raw,b.raw,f+".raw");
    Compare<std::decay_t<decltype(a.raw_follow_display)>>::run(a.raw_follow_display,b.raw_follow_display,f+".raw_follow_display");
    Compare<std::decay_t<decltype(a.raw_max_pixels)>>::run(a.raw_max_pixels,b.raw_max_pixels,f+".raw_max_pixels");
    Compare<std::decay_t<decltype(a.roi_sampling_policy)>>::run(a.roi_sampling_policy,b.roi_sampling_policy,f+".roi_sampling_policy");
    Compare<std::decay_t<decltype(a.tissue_grid)>>::run(a.tissue_grid,b.tissue_grid,f+".tissue_grid");
}};
inline ProjectBatchOptions previous_run(const ProjectBatchOptions& options_,bool use_image_settings_,const detail::WorkflowSnapshot& w,const std::array<char,128>& raw_x_,const std::array<char,128>& raw_y_) {
using detail::reset_cell_detector;
using detail::gui_raw_axis;
const auto& tissue=w.tissue;
const auto& tissue_grid=w.tissue_grid;
const auto& tissue_min_area=w.tissue_min_area;
const auto& tissue_max_area=w.tissue_max_area;
const auto& cells=w.cells;
const auto& roi_kind=w.roi_kind;
const auto& roi_sampling_policy=w.roi_sampling_policy;
const auto& nucleus=w.nucleus;
const auto& cell_grid=w.cell_grid;
const auto& cell_nominal=w.cell_nominal;
const auto& grid_cells=w.grid_cells;
const auto& cell_min_area=w.cell_min_area;
const auto& cell_max_area=w.cell_max_area;
const auto& pixel_model=w.pixel_model;
                auto run_options=options_;
                if(run_options.kind==ProjectBatchKind::RawPixels || run_options.kind==ProjectBatchKind::RawPixelContours) {
                    if(!*raw_x_.data() && !*raw_y_.data()) {run_options.raw_pixel_x.reset();run_options.raw_pixel_y.reset();}
                    else {if(!*raw_x_.data() || !*raw_y_.data()) throw std::invalid_argument("enter both raw axes");run_options.raw_pixel_x=gui_raw_axis(raw_x_.data());run_options.raw_pixel_y=gui_raw_axis(raw_y_.data());}
                    run_options.nucleus=false;
                }
                else if(run_options.kind==ProjectBatchKind::Pixels) {if(!run_options.pixel_model && pixel_model) run_options.pixel_model=*pixel_model;run_options.nucleus=false;}
                else if(use_image_settings_ && run_options.kind!=ProjectBatchKind::Raw) {
                    run_options.tissue=tissue;
                    if(run_options.kind==ProjectBatchKind::Tissue) {run_options.tissue_grid=tissue_grid;run_options.tissue_min_component_area_um2=tissue_min_area;run_options.tissue_max_component_area_um2=tissue_max_area;}run_options.cells=cells;run_options.roi_kind=roi_kind;run_options.roi_sampling_policy=roi_sampling_policy;
                    run_options.nucleus=(run_options.kind==ProjectBatchKind::Cells || run_options.tma_only)?false:nucleus;
                    if(run_options.kind==ProjectBatchKind::Cells) {
                        run_options.cell_grid=cell_grid;run_options.cell_nominal=cell_nominal;run_options.grid_cells=grid_cells;
                        run_options.cell_min_nucleus_area_um2=(cell_grid || cell_nominal)?std::nullopt:cell_min_area;run_options.cell_max_nucleus_area_um2=(cell_grid || cell_nominal)?std::nullopt:cell_max_area;
                    }
                }
                if(run_options.kind!=ProjectBatchKind::Raw) {run_options.raw_grid.reset();run_options.raw_grid_work_bytes=256u*1024*1024;}
                if(run_options.kind!=ProjectBatchKind::Roi) run_options.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
                if(run_options.kind!=ProjectBatchKind::Tissue) {run_options.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;run_options.tissue.smoothing_sigma=0;run_options.tissue.median_cleanup=false;}
                if(run_options.kind!=ProjectBatchKind::Cells) {reset_cell_detector(run_options.cells);run_options.cell_grid.reset();run_options.cell_nominal.reset();}
                if(run_options.cell_nominal) run_options.cells=CellParameters{};
return run_options;
}
}
