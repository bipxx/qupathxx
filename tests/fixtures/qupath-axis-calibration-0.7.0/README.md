# Pinned saved Z/time calibration

Ten saved-data entries share one coordinate-coded TZCYX source. Saved values
deliberately differ from source calibration. `saved-reference.json` reads the
exact binary through QuPath without a supplied project/source builder. Tests
also declare physical expected values independently. The ordinary project-entry
`reference.json` is retained to show builder substitution for the seven reused
entries, and is not the saved-state oracle.

The first capture saved seven TimeUnit cases then failed on NaN timestamps;
the second resume failed at parsing. The third reused those seven saved binaries
byte-for-byte and saved finite partial, nonmonotonic and unknown calibration.
Manifest commands and frozen executed probes preserve each attempt. The incomplete
NaN entry is excluded; NaN serialization is an observed upstream refusal.
These are engineering references, not native-product acceptance or biology.
