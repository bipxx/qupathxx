# Authored DICOM WSI handoff fixture

These three 1,118-byte files are synthetic RGB fixtures from the local two-instance
concatenation check. They contain generated pixels, synthetic UIDs and no patient
or scanner data. They are project-authored test material under AGPL-3.0-only.

The original two-frame fixture used reversible JPEG 2000. The fixture authoring
script split its two encoded frames into separate instances, set the concatenation
frame offsets to 0 and 1, and retained the common series and concatenation UIDs.
`changed-instance.dcm` retains the second instance's metadata and replaces its
frame with the first instance's encoded frame. This keeps the file size, series
UID, frame geometry and primary instance unchanged for the refusal test.

| File | SHA-256 |
| --- | --- |
| `z_part1.dcm` | `ed695384810299a5161cd38babda86103aaff85711bf25daa8c50fd789d964c7` |
| `a_part2.dcm` | `b8ee60fea2860f344600ad147710279e91c16883dd78ae06458b1724e015f1cb` |
| `changed-instance.dcm` | `a126a53b0ebb454f0637d02b72d4be31df497d776f11bc682ac80b58cc8b5c62` |

The original pair yields PAM SHA-256
`0acd09b59e5c345d2965262bf4aa10205dde048f4653f07df15111d49e649df1`
for the level-zero crop at XY (13, 4), width 12 and height 6.
