"""Expected current project wire format, independent of native implementation."""
import os


# Frozen SDK regressions can pin their documented schema explicitly. Keeping
# the expected version independent of the database makes writer regressions fail.
CURRENT_SCHEMA_VERSION = int(os.environ.get('QUPATHXX_TEST_PROJECT_SCHEMA', '30'))


def drop_newer_project_tables(database, version):
    """Remove post-schema26 table additions from synthetic legacy fixtures.

    Earlier formats retain their existing format-specific fixture edits.
    Add subsequent table migrations here, rather than to every old fixture.
    Deliberate corruption/downgrade rejection tests do not use this helper.
    """
    if version < 30:
        database.execute('DROP TABLE IF EXISTS plugin_links')
    if version < 29:
        database.execute('DROP TABLE IF EXISTS spatial_density_records')
    if version < 28:
        database.execute('DROP TABLE IF EXISTS spatial_neighborhood_records')
    if version < 27:
        database.execute('DROP TABLE IF EXISTS analysis_statistics')
