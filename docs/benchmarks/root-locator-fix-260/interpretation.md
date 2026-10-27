# Namespace-stable serialized locators

At qualified a5b5394, four new regression configurations fail: paired public
snapshots with existing/missing linked sources, mixed RGB/raw historical stream
links and actual moved-output checkpoint reuse. At f9ad28f all four pass. The
repaired witness produces identical whole-file hashes and relative image
locators for ordinary/extended output-root spelling.

One private relative_locator helper is shared by the image, QSL1 and QWL1
serializers. It compares ordinary DOS/UNC roots without filesystem lookup or
changing physical access paths. Incompatible drives/shares and literal/device
name cases retain absolute fallback. Missing sources need no filesystem lookup.
Existing source, model, checkpoint and publication guards are retained.

Windows naming rules distinguish ordinary paths from extended/literal names;
the controls cover trailing dots/spaces, reserved names, streams and device
namespaces. See [Microsoft naming rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)
and [extended path limits](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation).
UNC controls here are lexical and do not claim network-share runtime support.

All local cohorts, typed admission, package/source/SDK checks pass. Candidate260
is frozen for the complete platform roles; candidate258 success does not transfer.
The scripts are archived from their original build/ci paths. Binary products and
raw paired snapshot databases remain in the local retained packet.
