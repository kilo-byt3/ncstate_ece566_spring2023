#include <fstream>
#include <memory>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unordered_map>
#include<iostream>

#include "llvm-c/Core.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/CallGraph.h"
//#include "llvm/Analysis/AnalysisManager.h"

#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/SourceMgr.h"
#include <memory>

using namespace llvm;

static void DoInlining(Module *);

static void summarize(Module *M);

static void print_csv_file(std::string outputfile);

static cl::opt<std::string>
        InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::Required, cl::init("-"));

static cl::opt<std::string>
        OutputFilename(cl::Positional, cl::desc("<output bitcode>"), cl::Required, cl::init("out.bc"));

static cl::opt<bool>
        InlineHeuristic("inline-heuristic",
              cl::desc("Use student's inlining heuristic."),
              cl::init(false));

static cl::opt<bool>
        InlineConstArg("inline-require-const-arg",
              cl::desc("Require function call to have at least one constant argument."),
              cl::init(false));

static cl::opt<int>
        InlineFunctionSizeLimit("inline-function-size-limit",
              cl::desc("Biggest size of function to inline."),
              cl::init(200));

static cl::opt<int>
        InlineGrowthFactor("inline-growth-factor",
              cl::desc("Largest allowed program size increase factor (e.g. 2x)."),
              cl::init(20));


static cl::opt<bool>
        NoInline("no-inline",
              cl::desc("Do not perform inlining."),
              cl::init(false));


static cl::opt<bool>
        NoPreOpt("no-preopt",
              cl::desc("Do not perform pre-inlining optimizations."),
              cl::init(false));

static cl::opt<bool>
        NoPostOpt("no-postopt",
              cl::desc("Do not perform post-inlining optimizations."),
              cl::init(false));

static cl::opt<bool>
        Verbose("verbose",
                    cl::desc("Verbose stats."),
                    cl::init(false));

static cl::opt<bool>
        NoCheck("no",
                cl::desc("Do not check for valid IR."),
                cl::init(false));


static llvm::Statistic nInstrBeforeOpt = {"", "nInstrBeforeOpt", "number of instructions"};
static llvm::Statistic nInstrBeforeInline = {"", "nInstrPreInline", "number of instructions"};
static llvm::Statistic nInstrAfterInline = {"", "nInstrAfterInline", "number of instructions"};
static llvm::Statistic nInstrPostOpt = {"", "nInstrPostOpt", "number of instructions"};


static void countInstructions(Module *M, llvm::Statistic &nInstr) {
  for (auto i = M->begin(); i != M->end(); i++) {
    for (auto j = i->begin(); j != i->end(); j++) {
      for (auto k = j->begin(); k != j->end(); k++) {
	nInstr++;
      }
    }
  }
}


int main(int argc, char **argv) {
    // Parse command line arguments
    cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");

    // Handle creating output files and shutting down properly
    llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
    LLVMContext Context;

    // LLVM idiom for constructing output file.
    std::unique_ptr<ToolOutputFile> Out;
    std::string ErrorInfo;
    std::error_code EC;
    Out.reset(new ToolOutputFile(OutputFilename.c_str(), EC,
                                 sys::fs::OF_None));

    EnableStatistics();

    // Read in module
    SMDiagnostic Err;
    std::unique_ptr<Module> M;
    M = parseIRFile(InputFilename, Err, Context);

    // If errors, fail
    if (M.get() == 0)
    {
        Err.print(argv[0], errs());
        return 1;
    }

    countInstructions(M.get(),nInstrBeforeOpt);
    
    if (!NoPreOpt) {
      legacy::PassManager Passes;
      Passes.add(createPromoteMemoryToRegisterPass());    
      Passes.add(createEarlyCSEPass());
      Passes.add(createSCCPPass());
      Passes.add(createAggressiveDCEPass());
      Passes.add(createVerifierPass());
      Passes.run(*M);  
    }

    countInstructions(M.get(),nInstrBeforeInline);    

    if (!NoInline) {
        DoInlining(M.get());
    }

    countInstructions(M.get(),nInstrAfterInline);
    
    if (!NoPostOpt) {
      legacy::PassManager Passes;
      Passes.add(createPromoteMemoryToRegisterPass());    
      Passes.add(createEarlyCSEPass());
      Passes.add(createSCCPPass());
      Passes.add(createAggressiveDCEPass());
      Passes.add(createVerifierPass());
      Passes.run(*M);  
    }

    countInstructions(M.get(),nInstrPostOpt);
    
    // Collect statistics on Module
    summarize(M.get());
    print_csv_file(OutputFilename);

    if (Verbose)
        PrintStatistics(errs());

    // Verify integrity of Module, do this by default
    if (!NoCheck)
    {
        legacy::PassManager Passes;
        Passes.add(createVerifierPass());
        Passes.run(*M.get());
    }

    // Write final bitcode
    WriteBitcodeToFile(*M.get(), Out->os());
    Out->keep();

    return 0;
}

static llvm::Statistic nFunctions = {"", "Functions", "number of functions"};
static llvm::Statistic nInstructions = {"", "Instructions", "number of instructions"};
static llvm::Statistic nLoads = {"", "Loads", "number of loads"};
static llvm::Statistic nStores = {"", "Stores", "number of stores"};

static void summarize(Module *M) {
    for (auto i = M->begin(); i != M->end(); i++) {
        if (i->begin() != i->end()) {
            nFunctions++;
        }

        for (auto j = i->begin(); j != i->end(); j++) {
            for (auto k = j->begin(); k != j->end(); k++) {
                Instruction &I = *k;
                nInstructions++;
                if (isa<LoadInst>(&I)) {
                    nLoads++;
                } else if (isa<StoreInst>(&I)) {
                    nStores++;
                }
            }
        }
    }
}

static void print_csv_file(std::string outputfile)
{
    std::ofstream stats(outputfile + ".stats");
    auto a = GetStatistics();
    for (auto p : a) {
        stats << p.first.str() << "," << p.second << std::endl;
    }
    stats.close();
}

static llvm::Statistic Inlined = {"", "Inlined", "Inlined a call."};
static llvm::Statistic ConstArg = {"", "ConstArg", "Call has a constant argument."};
static llvm::Statistic SizeReq = {"", "SizeReq", "Call has a constant argument."};


#include "llvm/Transforms/Utils/Cloning.h"

// Function to count the number of instructions in the function at the call instruction callInst


int numInstructions(CallInst * callInst)
{
  Function * Callee = callInst->getCalledFunction();
  int num=0;
  if(Callee)
    for (auto bb = Callee->begin();bb!=Callee->end();bb++)
      for(auto instr = bb->begin(); instr!= bb->end(); instr++)
        num++;
  return num;
}

int numInstructions(Module * M)
{
  int num=0;
  for (auto F = M->begin();F!=M->end();F++)
    for(auto BB = F->begin();BB!=F->end();BB++)
      for (auto I = BB->begin();I!=BB->end();I++)
        num++;
  return num;
}

int numFunctions(Module * M)
{
  int num = 0;
  for(auto &F : *M)
    for(auto &BB : F)
      for(auto &I : BB)
      {
        CallInst* callInst = dyn_cast<CallInst>(&I);
        if (callInst)
          num++;
      }
  return num;
}

int numLoads(Module * M)
{
  int num = 0;
  for(auto &F : *M)
    for(auto &BB : F)
      for(auto &I : BB)
      {
        auto *loadInst = dyn_cast<LoadInst>(&I);
        if (loadInst) 
          num++;
      }
  return num;
}

int numStores(Module * M)
{
  int num = 0;
  for(auto &F : *M)
    for(auto &BB : F)
      for(auto &I : BB)
      {
        auto storeInst = dyn_cast<StoreInst>(&I);
        if (storeInst)
          num++;
      }
  return num;
}

bool isRecursive(Function * Callee)
{
  for(auto& bb: *Callee)
  {
    for(auto& instr: bb)
    {
      if (auto* call = dyn_cast<CallInst>(&instr))
      {
        Function* callRec = call->getCalledFunction();
        if(callRec==Callee)
          return true;
      }
    }
  }
  return false;
}

bool hasAllConstArgs(CallInst * callInst)
{
  for (unsigned i = 0; i < callInst->getNumOperands(); i++) 
    if (!isa<Constant>(callInst->getOperand(i)))
      return false;
  return true;
}

bool hasAConstArg(CallInst * callInst)
{
  for (unsigned i = 0; i < callInst->getNumOperands(); i++) 
    if (isa<Constant>(callInst->getOperand(i)))
      return true;
  return false;
}


void minorStats(Module *M)
{
  errs()<<"Number of Instructions: "<<numInstructions(M)<<"\n";
  errs()<<"Number of Functions: "<<numFunctions(M)<<"\n";
  errs()<<"Number of Loads: "<<numLoads(M)<<"\n";
  errs()<<"Number of Store: "<<numStores(M)<<"\n";
}
  

static void DoInlining(Module *M)
{
  std::deque<CallInst *> worklist;
  std::set<CallInst *> inlined_calls;

  errs()<<"\n##############################################################\n";
  errs()<<"Before:\n";
  minorStats(M);

  int num = numInstructions(M);

  if(InlineHeuristic)
    {
    for (auto &F : *M) 
    {
      for (auto &BB : F) 
      {
        for (auto &I : BB) 
        {
          auto *CI = dyn_cast<CallInst>(&I);
          if (CI)
          {
            Function *Callee = CI->getCalledFunction();
            if (Callee && !Callee->isDeclaration())
            {
              if (numInstructions(CI)<=InlineFunctionSizeLimit )
              {
                if(!InlineConstArg || (InlineConstArg && hasAConstArg(CI)))
                  worklist.push_back(CI);
                if(InlineConstArg && hasAConstArg(CI))
                  ConstArg++;
              }
            }
          }
        }
      }
    }
    while (!worklist.empty())
    //while (!worklist.empty() && numInstructions(M)<(num*InlineGrowthFactor)) 
    {
      //errs()<<worklist.size()<<"\n";
      CallInst *CI = worklist.front();
      worklist.pop_front();
      /*
      if (inlined_calls.find(CI) != inlined_calls.end())
        continue;
        */
      Function *Callee = nullptr;
      if (CI)
      {
        Callee = CI->getCalledFunction();
        if (Callee && !Callee->isDeclaration())
        {
          InlineResult IR = isInlineViable(*Callee);
          if(IR.isSuccess())
          {
            InlineFunctionInfo IFI;
            auto Zone = CI->getParent();
            IR = InlineFunction(*CI, IFI);
            if (IR.isSuccess())
            {
                inlined_calls.insert(CI);
                Inlined++;
                //changed = true;
                for (auto &I: *Zone)
                {
                  auto *newCI = dyn_cast<CallInst>(&I);
                  if(newCI)
                    if(inlined_calls.find(newCI) == inlined_calls.end())
                      worklist.push_back(newCI);
                }
            }
          }
        }
      }
    }
    
    }
    errs()<<"After: \n";
    minorStats(M);
    errs()<<"##############################################################\n\n";
    SizeReq = num/numInstructions(M);
}
