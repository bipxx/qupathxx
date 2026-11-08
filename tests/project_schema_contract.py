"""Expected current project wire format, independent of native implementation."""
CURRENT_SCHEMA_VERSION=27


def drop_newer_project_tables(database, version):
    """Remove post-schema26 table additions from synthetic legacy fixtures.

    Earlier formats retain their existing format-specific fixture edits.
    Add subsequent table migrations here, rather than to every old fixture.
    Deliberate corruption/downgrade rejection tests do not use this helper.
    """
    if version < 27:
        database.execute("DROP TABLE IF EXISTS analysis_statistics")
