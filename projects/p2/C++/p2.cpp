#include <fstream>
#include <memory>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

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

#include "../C/dominance.h"

using namespace llvm;

static void CommonSubexpressionElimination(Module *);

static void summarize(Module *M);
static void print_csv_file(std::string outputfile);

static cl::opt<std::string>
        InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::Required, cl::init("-"));

static cl::opt<std::string>
        OutputFilename(cl::Positional, cl::desc("<output bitcode>"), cl::Required, cl::init("out.bc"));

static cl::opt<bool>
        Mem2Reg("mem2reg",
                cl::desc("Perform memory to register promotion before CSE."),
                cl::init(false));

static cl::opt<bool>
        NoCSE("no-cse",
              cl::desc("Do not perform CSE Optimization."),
              cl::init(false));

static cl::opt<bool>
        Verbose("verbose",
                    cl::desc("Verbose stats."),
                    cl::init(false));

static cl::opt<bool>
        NoCheck("no",
                cl::desc("Do not check for valid IR."),
                cl::init(false));

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

    // If requested, do some early optimizations
    if (Mem2Reg)
    {
        legacy::PassManager Passes;
        Passes.add(createPromoteMemoryToRegisterPass());
        Passes.run(*M.get());
    }

    if (!NoCSE) {
        CommonSubexpressionElimination(M.get());
    }

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

static llvm::Statistic CSEDead = {"", "CSEDead", "CSE found dead instructions"};
static llvm::Statistic CSEElim = {"", "CSEElim", "CSE redundant instructions"};
static llvm::Statistic CSESimplify = {"", "CSESimplify", "CSE simplified instructions"};
static llvm::Statistic CSELdElim = {"", "CSELdElim", "CSE redundant loads"};
static llvm::Statistic CSEStore2Load = {"", "CSEStore2Load", "CSE forwarded store to load"};
static llvm::Statistic CSEStElim = {"", "CSEStElim", "CSE redundant stores"};

bool isDead(Instruction &I) {

  int opcode = I.getOpcode();
  switch(opcode){
  case Instruction::Add:
  case Instruction::FNeg:
  case Instruction::FAdd: 	
  case Instruction::Sub:
  case Instruction::FSub: 	
  case Instruction::Mul:
  case Instruction::FMul: 	
  case Instruction::UDiv:	
  case Instruction::SDiv:	
  case Instruction::FDiv:	
  case Instruction::URem: 	
  case Instruction::SRem: 	
  case Instruction::FRem: 	
  case Instruction::Shl: 	
  case Instruction::LShr: 	
  case Instruction::AShr: 	
  case Instruction::And: 	
  case Instruction::Or: 	
  case Instruction::Xor: 	
  case Instruction::Alloca:
  case Instruction::GetElementPtr: 	
  case Instruction::Trunc: 	
  case Instruction::ZExt: 	
  case Instruction::SExt: 	
  case Instruction::FPToUI: 	
  case Instruction::FPToSI: 	
  case Instruction::UIToFP: 	
  case Instruction::SIToFP: 	
  case Instruction::FPTrunc: 	
  case Instruction::FPExt: 	
  case Instruction::PtrToInt: 	
  case Instruction::IntToPtr: 	
  case Instruction::BitCast: 	
  case Instruction::AddrSpaceCast: 	
  case Instruction::ICmp: 	
  case Instruction::FCmp: 	
  case Instruction::PHI: 
  case Instruction::Select: 
  case Instruction::ExtractElement: 	
  case Instruction::InsertElement: 	
  case Instruction::ShuffleVector: 	
  case Instruction::ExtractValue: 	
  case Instruction::InsertValue: 
    if ( I.use_begin() == I.use_end() )
         {
	       return true;
         }
         break;

  case Instruction::Load:
    {
      LoadInst *li = dyn_cast<LoadInst>(&I);
      if (li && li->isVolatile())
	   return false;
      if (I.use_begin() == I.use_end())
         return true;
      break;
      
    }
  
  default: 
    // any other opcode fails 
      return false;
  }

  
  return false;
}

bool isCommon (Instruction* instr1, Instruction* instr2)
{
    if (LLVMDominates(wrap(instr1->getFunction()), wrap(instr1->getParent()), wrap(instr2->getParent())))
    {
        if (isa<LoadInst>(instr1) || isa<StoreInst>(instr1) || isa<BranchInst>(instr1) || isa<AllocaInst>(instr1))
            return false;
        if (isa<LoadInst>(instr2) || isa<StoreInst>(instr2) || isa<BranchInst>(instr2) || isa<AllocaInst>(instr2))
            return false;
        if ((instr1->getOpcode() == instr2->getOpcode()) && (instr1->getType() == instr2->getType()) && (instr1->getNumOperands() == instr2->getNumOperands()))
        {
            for (unsigned int i=0;i< instr1->getNumOperands();i++)
                if (instr1->getOperand(i) != instr2->getOperand(i))
                    return false;
            return true;
        }
    }
    return false;
}

void deadCodeEliminationLight(Module* M)
{
    for (auto func = M->begin();func!=M->end();func++)
    {
        for (auto bb = func->begin();bb!=func->end();bb++)
            for (auto instr = bb->begin();instr!=bb->end();)
                if (isDead(*instr))
                {
                    auto toErase = instr;
                    instr++;
                    toErase->eraseFromParent();
                    CSEDead++;
                }
                else instr++;
    }
}

void simplify(Module* M)
{
    for (auto func = M->begin();func!=M->end();func++)
    {
        for (auto bb = func->begin();bb!=func->end();bb++)
            for (auto instr = bb->begin();instr!=bb->end();)
            {
                Value *val = SimplifyInstruction(&*instr,M->getDataLayout());
                if (val != nullptr) 
                {
                    instr->replaceAllUsesWith(val);
                    auto toErase = instr;
                    instr++;
                    toErase->eraseFromParent();
                    CSESimplify++;
                }
                else instr++;
            }
    }
}

void CSE(Module* M)
{
    for (auto func = M->begin();func!=M->end();func++)
    {
        for (auto bb = func->begin();bb!=func->end();bb++)
            for (auto instr1 = bb->begin();instr1!=bb->end();instr1++)
            {
                auto instr2 = instr1;
                instr2++;
                while(instr2!=bb->end())
                    if (isCommon(&*instr1, &*instr2))
                    {
                        auto toErase = instr2;
                        instr2++;
                        toErase->replaceAllUsesWith((Value *)(&* instr1));
                        toErase->eraseFromParent();
                        CSEElim++;
                    }
                    else instr2++;

                instr2 = instr1;
                instr2++;

                auto parent = wrap(instr1->getParent());
                auto child = LLVMFirstDomChild(parent);
                
                while (child)
                {
                    for (auto instr2 = unwrap(child)->begin(); instr2 != unwrap(child)->end();)
                    {
                        if (isCommon(&*instr1, &*instr2))
                        {
                            auto toErase = instr2;
                            instr2++;
                            toErase->replaceAllUsesWith((Value *)(&* instr1));
                            toErase->eraseFromParent();
                            CSESimplify++;
                        }
                        else instr2++;
                    }
                    child = LLVMNextDomChild(parent,child);  // get next child of BB
                }
            }
    }
}

void loadElimination(Module* M)
{
    for (auto func = M->begin();func!=M->end();func++)
    {
        for (auto bb = func->begin();bb!=func->end();bb++)
        {
            for (auto instr1 = bb->begin();instr1!=bb->end();)
            {
                bool storeFound = false;
                if (isa<LoadInst>(instr1))
                {
                    auto instr2 = instr1;
                    instr2++;
                    while(instr2!=bb->end())
                    {
                        if (isa<LoadInst>(instr2) && !instr2->isVolatile() && (dyn_cast<LoadInst>(instr2)->getPointerOperand() == dyn_cast<LoadInst>(instr1)->getPointerOperand()) && (instr1->getType() == instr2->getType()))
                        {
                            //errs()<<"Load Comparison: "<<*instr1<<" | "<<*instr2<<"\n";
                            auto toErase = instr2;
                            instr2++;
                            toErase->replaceAllUsesWith((Value *)(&* instr1));
                            toErase->eraseFromParent();
                            CSELdElim++;
                            //LLVMStatisticsInc(CSE_RLoad);
                            continue;
                        }
                        else if (isa<StoreInst>(instr2) && !storeFound)
                        {
                            storeFound = true;
                            break;
                        }
                        instr2++;
                    }
                    instr1++;
                    if (storeFound) continue;
                }
                else instr1++;
            }
        }
    }
}

void storeElimination(Module* M)
{
    for (auto func = M->begin();func!=M->end();func++)
    {
        for (auto bb = func->begin();bb!=func->end();bb++)
        {
            for (auto instr1 = bb->begin();instr1!=bb->end();)
            {
                bool storeFound = false;
                if (isa<StoreInst>(instr1))
                {
                    auto instr2 = instr1;
                    instr2++;
                    while(instr2!=bb->end())
                    {
                        if (isa<LoadInst>(instr2) && !instr2->isVolatile() && (dyn_cast<LoadInst>(instr2)->getPointerOperand() == dyn_cast<StoreInst>(instr1)->getPointerOperand()) && (((dyn_cast<StoreInst>(instr1))->getValueOperand())->getType() == instr2->getType()))
                        {
                            auto toErase = instr2;
                            instr2++;
                            CSEStore2Load++;
                            toErase->replaceAllUsesWith((dyn_cast<StoreInst>(instr1))->getValueOperand());
                            toErase->eraseFromParent();
                            continue;
                        }
                        else if ( isa<StoreInst>(instr2) && (dyn_cast<StoreInst>(instr2)->getPointerOperand() == dyn_cast<StoreInst>(instr1)->getPointerOperand()) && (((dyn_cast<StoreInst>(instr1))->getValueOperand())->getType()==((dyn_cast<StoreInst>(instr2))->getValueOperand())->getType()) )
                        {

                            auto toErase = instr1;
                            instr1++;
                            toErase->eraseFromParent();
                            CSEStElim++;
                            storeFound = true;
                            break;
                        }
                        instr2++;
                    }
                    instr1++;
                    if (storeFound) continue;
                }
                else instr1++;
            }
        }
    }
}

static void CommonSubexpressionElimination(Module *M) 
{
    // Implement this function

    if (M!=nullptr)
    {        
        // optimization 0 - Deadcode Elimination
        deadCodeEliminationLight(M);

        // optimization 1.a - Simplify Instructions
        simplify(M);

        // optimization 1.b - CSE
        CSE(M);

        // optimization 2 - Load Eliminations
        loadElimination(M);

        // optimization 2.b - Simplify Instructions 2
        //simplify(M);

        // optimization 3.a - Store Eliminations
        storeElimination(M);

        // optimization 3.b - Simplify Instructions 3
        simplify(M);
    }
}

