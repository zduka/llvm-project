#include <iostream>

#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/Binary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Program.h"


#include "bolt/Utils/CommandLineOpts.h"


//#include "bolt/Core/BinaryContext.h"
#include "bolt/Rewrite/RewriteInstance.h"

#define DEBUG_TYPE "bolt"

using namespace llvm;
using namespace object;
using namespace bolt;

static constexpr char const * TOOL_NAME = "llvm-bolt-cfg";

namespace opts {

  static cl::opt<std::string> InputFilename(cl::Positional,
                                            cl::desc("<executable>"),
                                            cl::Required, cl::cat(BoltCategory),
                                            cl::sub(cl::SubCommand::getAll()));
  // from RewriteInstance.cpp
  extern cl::opt<bool> PrintCFG;
  extern cl::opt<bool> PrintLoopInfo;
  extern cl::opt<bool> DumpDotAll;
  // from BinaryFunction.cpp
  extern cl::opt<bool> DotToolTipCode;
}

// TODO from llvm-bolt.cpp, duplicated code
static void report_error(StringRef Message, std::error_code EC) {
  assert(EC);
  errs() << TOOL_NAME << ": '" << Message << "': " << EC.message() << ".\n";
  exit(1);
}

// TODO from llvm-bolt.cpp, duplicated code
static void report_error(StringRef Message, Error E) {
  assert(E);
  errs() << TOOL_NAME << ": '" << Message << "': " << toString(std::move(E))
         << ".\n";
  exit(1);
}

// TODO from llvm-bolt.cpp, duplicated code
static std::string GetExecutablePath(const char *Argv0) {
  SmallString<256> ExecutablePath(Argv0);
  // Do a PATH lookup if Argv0 isn't a valid path.
  if (!llvm::sys::fs::exists(ExecutablePath))
    if (llvm::ErrorOr<std::string> P =
            llvm::sys::findProgramByName(ExecutablePath))
      ExecutablePath = *P;
  return std::string(ExecutablePath.str());
}


int main(int argc, char * argv[]) {
  std::string ToolPath = GetExecutablePath(argv[0]);

  // Initialize targets and assembly printers/parsers.
  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllDisassemblers();

  InitializeAllTargets();
  InitializeAllAsmPrinters();

  cl::HideUnrelatedOptions(opts::BoltCategory);
  cl::HideUnrelatedOptions(opts::BoltDiffCategory);
  cl::HideUnrelatedOptions(opts::BoltOptCategory);
  cl::HideUnrelatedOptions(opts::BoltRelocCategory);
  cl::HideUnrelatedOptions(opts::BoltOutputCategory);
  cl::HideUnrelatedOptions(opts::AggregatorCategory);
  cl::HideUnrelatedOptions(opts::BoltInstrCategory);
  cl::HideUnrelatedOptions(opts::HeatmapCategory);
  opts::PrintCFG = true;

  opts::PrintLoopInfo.setHiddenFlag(cl::NotHidden);
  opts::DumpDotAll.setHiddenFlag(cl::NotHidden);
  opts::DotToolTipCode.setHiddenFlag(cl::NotHidden);

  // set values for the command-line arguments we are not using and hide them from the 
  opts::DiffOnly = true;

  cl::ParseCommandLineOptions(argc, argv,
                              "BOLT - Binary Optimization and Layout Tool\n");



  // process the opened elf file, get the text section an output its disassebly cfg
  // TODO try linking as executable if we fail the first time...
  auto TryDisasm = [&](llvm::StringRef InputFilename) {
    Expected<OwningBinary<Binary>> BinaryOrErr =
        createBinary(opts::InputFilename);
    if (Error E = BinaryOrErr.takeError())
      report_error(opts::InputFilename, std::move(E));
    Binary &Binary = *BinaryOrErr.get().getBinary();
    if (auto *e = dyn_cast<ELFObjectFileBase>(&Binary)) {
      auto RIOrErr = RewriteInstance::create(e, argc, argv, ToolPath);
      if (Error E = RIOrErr.takeError())
        return E;
        //report_error(opts::InputFilename, std::move(E));
      RewriteInstance &RI = *RIOrErr.get();
      return RI.run();
    } else if (dyn_cast<MachOObjectFile>(&Binary)) {
      // TODO: bolt also supports mach, let's do ELF first
    } else {
      report_error(opts::InputFilename, object_error::invalid_file_type);
    }
  };
  // if there was an error disassembling the given file, its likely because it was not executable
  if (Error E = TryDisasm(opts::InputFilename)) {
    // TODO 
    report_error(opts::InputFilename, std::move(E));
  }
  return EXIT_SUCCESS;
}