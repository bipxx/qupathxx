#pragma once
#include <string>
#include <vector>
namespace qpx {
// Filesystem publication policy; never part of scientific request identity.
enum class ExportWritePolicy { CreateNew, ReplaceExisting };
}
