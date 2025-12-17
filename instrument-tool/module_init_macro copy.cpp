#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;

// --- 1. The Visitor: Look for the specific function ---
class ModuleInitVisitor : public RecursiveASTVisitor<ModuleInitVisitor> {
public:
  explicit ModuleInitVisitor(ASTContext *Context) : Context(Context) {}

  bool VisitFunctionDecl(FunctionDecl *Func) {
    // We only care about "my_pci_init"
    if (Func->getNameAsString() == "my_pci_init") {
      
      llvm::outs() << "\n[+] Found Target Function: " << Func->getNameAsString() << "\n";
      llvm::outs() << "    Return Type: " << Func->getReturnType().getAsString() << "\n";

      // Check if it has a body
      if (Func->hasBody()) {
        llvm::outs() << "    Status: Has Body\n";
        
        // Let's dump the statements inside to prove we parsed it
        CompoundStmt *Body = dyn_cast<CompoundStmt>(Func->getBody());
        if (Body) {
          llvm::outs() << "    Block content:\n";
          for (auto *Stmt : Body->body()) {
             llvm::outs() << "      - " << Stmt->getStmtClassName() << "\n";
             
             // If it's a call, print what it calls (e.g., pci_register_driver)
             if (CallExpr *Call = dyn_cast<CallExpr>(Stmt)) {
               if (FunctionDecl *Callee = Call->getDirectCallee()) {
                 llvm::outs() << "        (Call to: " << Callee->getNameAsString() << ")\n";
               }
             }
             // If it's a return statement
             if (isa<ReturnStmt>(Stmt)) {
                 llvm::outs() << "        (Return Statement)\n";
             }
          }
        }
      }
    }
    return true;
  }

private:
  ASTContext *Context;
};

// --- 2. The Consumer ---
class ModuleInitConsumer : public ASTConsumer {
public:
  explicit ModuleInitConsumer(ASTContext *Context) : Visitor(Context) {}

  virtual void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  ModuleInitVisitor Visitor;
};

// --- 3. The Action ---
class ModuleInitAction : public ASTFrontendAction {
public:
  virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return std::make_unique<ModuleInitConsumer>(&Compiler.getASTContext());
  }
};

// --- 4. Main Driver ---
static llvm::cl::OptionCategory MyToolCategory("driver-parser options");

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

  // === THE MAGIC: MOCKING THE KERNEL ENVIRONMENT ===
  
  // 1. Define __init and __exit as empty (kernel macros)
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-D__init=", ArgumentInsertPosition::BEGIN));
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-D__exit=", ArgumentInsertPosition::BEGIN));
  
  // 2. Mock specific types. We inject a header content using -include
  // We create a temporary "mock_types.h" in memory essentially
  // Note: For simplicity, we just inject code via -include-pch or just write a small file.
  // Here, the easiest way is to add a flag that includes a local mock file we will create.
  
  // 3. Ignore missing files so the include <linux/...> doesn't crash us hard
  // Note: Clang will error but recover if we tell it to via "-ferror-limit=0"
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-ferror-limit=0", ArgumentInsertPosition::BEGIN));
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w", ArgumentInsertPosition::BEGIN)); // No warnings

  return Tool.run(newFrontendActionFactory<ModuleInitAction>().get());
}