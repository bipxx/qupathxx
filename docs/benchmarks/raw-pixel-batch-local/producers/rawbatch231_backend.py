from pathlib import Path
import os
from scikit_build_core.build import build_wheel
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159'
os.chdir(s)
settings={
    'build-dir':str(r/'build/ci/rawtraining218-windows'),
    'build.targets':['_native','qupathxx_region','qupathxx_batch','qupathxx_project_batch_tests','qupathxx_pixel_workflow_tests','qupathxx_pixel_feature_tests','qupathxx_classifier_tests'],
}
for name,value in dict(PYTHON='ON',GUI='OFF',GEOS='OFF',OPENSLIDE='OFF',TIFF='ON',PROJECT='ON',CLASSIFIER_TRAINING='ON',TESTS='ON',INTERCHANGE='ON').items():
    settings['cmake.define.QUPATHXX_BUILD_'+name]=value
settings['cmake.define.OpenCV_DIR']=str(r/'build/deps/opencv-sdk')
settings['cmake.define.QUPATHXX_OPENCV_NOTICES']=str(r/'build/deps/opencv-sdk/share/licenses/opencv')
print(build_wheel(str(r/'build/rawbatch231-final/wheel'),settings),flush=True)
