from pathlib import Path
root=Path(__file__).resolve().parents[2]
text=(root/'build/ci/linux_nominal_frontends294.py').read_text()
text=text.replace('nominal-cell-frontends-294','reader-locale-297').replace('nominal294','reader297').replace("commit.startswith('bb190eb7')","commit.startswith('28c33573')").replace('len(proof)==424','len(proof)==425')
text=text.replace("targets=['qupathxx_resampling_tests','qupathxx_cell_tests','qupathxx_cell_record_tests','qupathxx_object_classifier_tests','qupathxx_log_cell_recipe_tests','qupathxx_stains_tests','qupathxx_project_batch_tests','qupathxx_viewer_session_tests']","targets=['qupathxx_raw_image_tests','qupathxx_region']")
text=text.replace('^(resampling|cells|cell_records|object_classifier|log_cell_recipe|stains|project_batch|viewer_session)$','^raw_image$')
text=text.replace('Eight affected native suites passed under ASan/UBSan; nine products','Affected raw-image suite passed under ASan/UBSan; three products')
assert 'nominal294' not in text and "targets=['qupathxx_raw_image_tests','qupathxx_region']" in text
target=root/'build/ci/linux_reader_locale297.py';assert not target.exists();target.write_text(text,encoding='utf-8')
(root/'build/reader-locale-297/linux').mkdir(exist_ok=False)
