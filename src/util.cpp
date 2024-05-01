#include "util.h"
#include <llvm/Demangle/Demangle.h>

std::string getDemangledName(std::string_view Name) {
  char *DemangledName = llvm::itaniumDemangle(Name);
  return DemangledName ? DemangledName : Name.data();
}
