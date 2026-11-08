from pathlib import Path
import os
from scikit_build_core.build import build_wheel
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';os.chdir(s);os.environ["CMAKE_BUILD_PARALLEL_LEVEL"]="2"
settings={'build-dir':str(r/'build/ci/gridfrontends167-package'),'build.targets':['_native','qupathxx_region','qupathxx_batch','qupathxx_app','qupathxx_qupath_project_tests','qupathxx_project_tests','qupathxx_interchange_tests','qupathxx_spatial_record_tests','qupathxx_file_fingerprint_tests','qupathxx_object_classifier_tests','qupathxx_selection_tests']}
settings['cmake.define.FETCHCONTENT_SOURCE_DIR_IMGUI']=str(r/'build/gui-fonts-282/inputs/imgui193')
for name,value in dict(PYTHON='ON',GUI='ON',GEOS='ON',OPENSLIDE='ON',TIFF='ON',PROJECT='ON',CLASSIFIER_TRAINING='ON',TESTS='ON',INTERCHANGE='ON').items():settings['cmake.define.QUPATHXX_BUILD_'+name]=value
settings['cmake.define.OpenCV_DIR']=str(r/'build/deps/opencv-sdk');settings['cmake.define.QUPATHXX_OPENCV_NOTICES']=str(r/'build/deps/opencv-sdk/share/licenses/opencv')
settings['cmake.define.GEOS_DIR']=str(r/'build/deps/geos-sdk/lib/cmake/GEOS');settings['cmake.define.GEOS_LICENSE_FILE']=str(r/'build/deps/geos-sdk/share/licenses/geos/COPYING');settings['cmake.define.QUPATHXX_BUNDLE_GEOS']='OFF'
settings['cmake.define.OpenSlide_ROOT']=(r/'build/deps/openslide/openslide-bin-4.0.1.2-windows-x64').as_posix()
print(build_wheel(str(r/'build/object-edit-state-r5/wheel'),settings),flush=True)
