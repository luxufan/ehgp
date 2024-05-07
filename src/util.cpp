#include "util.h"
#include <llvm/Demangle/Demangle.h>

std::string getDemangledName(std::string_view Name) {
  char *DemangledName = llvm::itaniumDemangle(Name);
  if (DemangledName) {
    std::string S = DemangledName;
    std::free(DemangledName);
    return std::move(S);
  }
  return "";
}
