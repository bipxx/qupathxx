from pathlib import Path
import os
from scikit_build_core.build import build_wheel
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';os.chdir(s)
settings={'build-dir':str(r/'build/ci/gridfrontends167-package'),'build.targets':['_native', 'qupathxx_region', 'qupathxx_batch', 'qupathxx_app', 'qupathxx_project_batch_tests', 'qupathxx_pixel_workflow_tests', 'qupathxx_pixel_feature_tests', 'qupathxx_classifier_tests', 'qupathxx_viewer_session_tests', 'qupathxx_gui_replay_tests', 'qupathxx_overlay_tests', 'qupathxx_resampling_tests', 'qupathxx_tissue_contour_tests']}
for name,value in dict(PYTHON='ON',GUI='ON',GEOS='ON',OPENSLIDE='OFF',TIFF='ON',PROJECT='ON',CLASSIFIER_TRAINING='ON',TESTS='ON',INTERCHANGE='ON').items():settings['cmake.define.QUPATHXX_BUILD_'+name]=value
settings['cmake.define.OpenCV_DIR']=str(r/'build/deps/opencv-sdk');settings['cmake.define.QUPATHXX_OPENCV_NOTICES']=str(r/'build/deps/opencv-sdk/share/licenses/opencv')
settings['cmake.define.GEOS_DIR']=str(r/'build/deps/geos-sdk/lib/cmake/GEOS');settings['cmake.define.GEOS_LICENSE_FILE']=str(r/'build/deps/geos-sdk/share/licenses/geos/COPYING');settings['cmake.define.QUPATHXX_BUNDLE_GEOS']='OFF'
print(build_wheel(str(r/'build/raw-stream-batch-247/final-wheel'),settings),flush=True)
