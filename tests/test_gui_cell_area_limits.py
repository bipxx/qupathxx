"""Desktop requests use the same native physical-to-sampled resolver."""
import os

import numpy as np
import pytest
import qupathxx as q
from test_cell_calibration import scene
from test_gui_replay import event

pytestmark=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in desktop area limits')


def test_gui_keeps_the_same_resolved_cell_run_as_the_native_factory():
    doc,obj,base,_=scene()
    expected_request=q.CellRequest.with_nucleus_area_limits(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),10.1,11.25,base.parameters)
    expected=q.detect_cells(expected_request)
    options=q.GuiOptions();options.cell_parameters=base.parameters
    options.cell_min_nucleus_area_um2=10.1;options.cell_max_nucleus_area_um2=11.25
    options.analysis_vectors=[1,0,0,0,1,0]
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.DETECT_CELLS_KEY),
        event(12,q.GuiInputKind.KEEP_CELL_RUN_KEY,wait=q.GuiInputWait.CELLS_READY),
        event(14,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELLS_ARCHIVED)]
    report=q.show(doc,frame_limit=35,options=options)
    assert report.cell_state=='ready' and not report.cell_error
    assert report.cell_result.cell_count==expected.cell_count==1
    np.testing.assert_array_equal(report.cell_result.nuclei.labels_numpy(),expected.nuclei.labels_numpy())
    saved=doc.latest_cells(obj).result
    assert (saved.parameters.min_nucleus_pixels,saved.parameters.max_nucleus_pixels)==(81,90)
    assert saved.calibrated_feature(1).nucleus_sample_area_um2==11.25


def test_gui_rejects_invalid_physical_bounds_before_running_detection():
    doc,_,_,_=scene();options=q.GuiOptions();options.cell_min_nucleus_area_um2=-1
    with pytest.raises(ValueError,match='nonnegative'):q.show(doc,frame_limit=1,options=options)


@pytest.mark.skipif(not q.has_project,reason='project batch')
def test_gui_batch_preserves_original_physical_bounds_and_saved_pixel_recipe(tmp_path):
    doc,obj,base,_=scene();project=q.Project();image=project.add_document(doc)
    preset=q.ProjectBatchOptions();preset.kind=q.ProjectBatchKind.CELLS;preset.cells=base.parameters
    preset.cell_min_nucleus_area_um2=10.1;preset.cell_max_nucleus_area_um2=11.25
    native=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]),options=preset)
    options=q.GuiOptions();options.batch_options=preset;options.batch_output_directory=str(tmp_path/'batch')
    options.analysis_vectors=[1,0,0,0,1,0];options.replay_timeout_seconds=30
    options.inputs=[event(4,q.GuiInputKind.RUN_BATCH_KEY),event(8,q.GuiInputKind.MOVE,wait=q.GuiInputWait.BATCH_FINISHED)]
    report=q.show(project,frame_limit=35,options=options)
    assert report.batch_state=='succeeded' and not report.batch_error
    assert report.batch_result.parameters_sha256==native.parameters_sha256
    saved=q.Project.open(report.batch_result.images[0].path).document(image).latest_cells(obj).result
    assert saved.cell_count==1 and (saved.parameters.min_nucleus_pixels,saved.parameters.max_nucleus_pixels)==(81,90)
