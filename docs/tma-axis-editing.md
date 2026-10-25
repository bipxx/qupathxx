# TMA row and column corrections

This workflow extends the existing native TMA document. Source `96172f3` passes
[local native, installed and desktop checks](benchmarks/tma-axis-editing-local.json).
Platform qualification, automatic dearraying and biological acceptance remain open.

`Document.insert_tma_row(index, label="", bounds=None, missing=False)` and
`insert_tma_column` insert an axis before the zero-based index. Use the current
axis length to append. They return only new core IDs. Existing cores retain
IDs, names, geometry, case IDs and missing flags; their grid positions change.
New cores have empty case IDs. Each operation is one undoable native command.

With `bounds=None`, centers are interpolated between neighboring cores or
extrapolated at an edge; the closest core supplies the ellipse size. Sparse
lines can use spacing inferred from other occupied lines. Ambiguous spacing
rejects the entire command. A singleton axis has no inferred spacing. Supply
one `Rect` per cross-axis slot to place cores explicitly, or use `bounds=[]`
to insert vacant slots. Automatic placement does not move retained cores.
Axis labels must be unique on that axis; an empty label generates a unique
`Row N` or `Column N` label. Existing dimension and command budgets apply.

`remove_tma_row(index)` and `remove_tma_column(index)` delete that axis and its
cores, including ordinary descendant objects. A core belonging to a retained
axis survives, along with its descendants, even when manually nested under a
deleted core. It is reparented to the nearest surviving ancestor. Selected
cores nested within such a protected subtree are still deleted. Undo restores
the original hierarchy. Removing the final occupied cores clears the grid,
as with existing generic TMA deletion; there is no independent empty-grid store.

In the TMA panel, select a core or vacant slot and open **Grid edits**. Add a
row above/below or a column left/right of that slot. The optional label and
missing setting apply to new cores; turn off **Fill inserted slots with cores**
for vacant slots. Removal affects the selected row or column. Review deletion
of descendant objects before using these controls; Undo restores the command.

For a managed viewer use the same four method names on `ViewerSession`, with
`image_id` first. Submission copies bounds and labels; the owner resolves and
validates the operation. `expected_revision` rejects stale edits. Tickets use
the existing cancellation, command/edit limits and result retention. Consume
the ticket through `wait`/`take`; direct document mutation remains owner-only.
Project saves and QPTMA exports use the existing native persistence/codec paths.
Automatic dearraying and biological acceptance remain separate requirements.


The [qualification controls](benchmarks/tma-axis-auditor-controls.json) require
named native, installed, GUI/headless and typing outcomes. Missing cases,
duplicates, failures, substituted skip reasons and absent semantic markers
reject. These controls accept the retained local axis evidence and reject
older source191 evidence as proof of the later axis API. They do not substitute
for the required platform campaign or independent biological acceptance.
