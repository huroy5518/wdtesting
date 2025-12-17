#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include <fstream>
#include <vector>
#include <string>

using namespace clang;
using namespace clang::tooling;

// ... (Your existing Visitor/Consumer/Action classes go here) ...
Rewriter TheRewriter;
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// Option 1: Specific Output Filename (-o new_file.c)
static llvm::cl::opt<std::string> OutputFileOpt(
    "o", 
    llvm::cl::desc("Specify output filename (Single input file only)"),
    llvm::cl::cat(MyToolCategory)
);


// 我們要插入的宣告 (告訴編譯器這個函式在別的地方)
const char *ExternDecl = "\n/* Hook defined in other file */\nextern void create_test_debugfs(void);\n\n";

class InstrumentVisitor : public RecursiveASTVisitor<InstrumentVisitor> {
public:
  explicit InstrumentVisitor(ASTContext *Context) : Context(Context) {}

  bool VisitFunctionDecl(FunctionDecl *Func) {
    // 鎖定目標函式
    if (Func->getNameAsString() == "my_pci_init" && Func->hasBody()) {
      
      llvm::outs() << "[+] Processing: " << Func->getNameAsString() << "\n";

      // Action 1: 在函式「定義之前」插入宣告 (Prototype)
      // getBeginLoc() 是 "static int ..." 的位置
      TheRewriter.InsertText(Func->getBeginLoc(), ExternDecl, true, true);

      // Action 2: 在函式「return 之前」插入呼叫
      Stmt *Body = Func->getBody();
      if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Body)) {
        if (!CS->body_empty()) {
           // 取得最後一個語句 (通常是 return)
           Stmt *LastStmt = CS->body_back();
           
           // 在最後一個語句的開頭位置插入呼叫
           std::string CallCode = "    /* Tool Hook */\n    create_test_debugfs();\n    ";
           TheRewriter.InsertText(LastStmt->getBeginLoc(), CallCode, true, true);
        }
      }
    }
    return true;
  }

private:
  ASTContext *Context;
};

class InstrumentConsumer : public ASTConsumer {
public:
  explicit InstrumentConsumer(ASTContext *Context) : Visitor(Context) {}
  virtual void HandleTranslationUnit(ASTContext &Context) override {
    TheRewriter.setSourceMgr(Context.getSourceManager(), Context.getLangOpts());
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }
private:
  InstrumentVisitor Visitor;
};

class InstrumentAction : public ASTFrontendAction {
public:
  virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return std::make_unique<InstrumentConsumer>(&Compiler.getASTContext());
  }

  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    FileID MainFileID = SM.getMainFileID();
    const FileEntry *FE = SM.getFileEntryForID(MainFileID);
    
    if (!FE) return;
    
    std::string OldFilename = std::string(FE->getName());
    std::string NewFilename;

    // Logic: Decide output filename
    if (!OutputFileOpt.empty()) {
        // Mode A: User specified explicit -o filename
        NewFilename = OutputFileOpt;
    } else {
        // Mode B: Auto-generate using Suffix
        // Example: /path/to/source.c -> /path/to/source.instrumented.c
        
        // 1. Get Directory and Filename
        llvm::SmallString<128> PathVec(OldFilename);
        llvm::sys::path::remove_filename(PathVec); // now just directory
        
        StringRef BaseName = llvm::sys::path::stem(OldFilename); // "source"
        StringRef Extension = llvm::sys::path::extension(OldFilename); // ".c"
        
        // 2. Construct new name
        llvm::sys::path::append(PathVec, BaseName + Extension);
        NewFilename = PathVec.str().str();
    }

    // Write to file
    std::error_code EC;
    llvm::raw_fd_ostream OutFile(NewFilename, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "[!] Error writing to " << NewFilename << ": " << EC.message() << "\n";
        return;
    }

    TheRewriter.getEditBuffer(MainFileID).write(OutFile);
    llvm::outs() << "[*] Saved: " << NewFilename << "\n";
  }
};

// ==========================================
// Helper: Read flags from a file at runtime
// ==========================================
std::vector<std::string> LoadFlagsFromFile(const std::string &FilePath) {
    std::vector<std::string> Flags;
    std::ifstream File(FilePath);
    
    if (!File.is_open()) {
        llvm::errs() << "[!] Warning: Could not open flag file: " << FilePath << "\n";
        return Flags;
    }

    std::string Line;
    while (std::getline(File, Line)) {
        // Skip empty lines or comments
        if (Line.empty() || Line[0] == '#') continue;

        // Trim whitespace (simple implementation)
        size_t first = Line.find_first_not_of(" \t\r\n");
        size_t last = Line.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        
        std::string CleanFlag = Line.substr(first, (last - first + 1));
        Flags.push_back(CleanFlag);
    }
    
    llvm::outs() << "[*] Loaded " << Flags.size() << " flags from " << FilePath << "\n";
    return Flags;
}

// Blocklist to filter out bad GCC flags
bool isBlocked(const std::string &Arg) {
    static const std::vector<std::string> Blocklist = {
        "-fno-allow-store-data-races",
        "-fconserve-stack",
        "-femit-struct-debug-baseonly",
        "-mabi=lp64",
        "-fno-var-tracking-assignments"
    };
    for (const auto &Bad : Blocklist) {
        if (Arg == Bad) return true;
    }
    return false;
}

// ==========================================
// Main
// ==========================================

// Add a custom command line option to specify the flag file
static llvm::cl::opt<std::string> FlagFileOpt(
    "flags-file", 
    llvm::cl::desc("Path to a text file containing compiler flags"),
    llvm::cl::cat(MyToolCategory)
);


int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  ClangTool Tool(ExpectedParser.get().getCompilations(), ExpectedParser.get().getSourcePathList());

  // 1. Load flags dynamically if the user provided --flags-file
  std::vector<std::string> RuntimeFlags;
  if (!FlagFileOpt.empty()) {
      RuntimeFlags = LoadFlagsFromFile(FlagFileOpt);
  }

  // 2. Register the Adjuster
  Tool.appendArgumentsAdjuster(
      [&](const CommandLineArguments &Args, StringRef Filename) {
          CommandLineArguments AdjustedArgs;

          // A. Keep original args (filtering bad ones)
          for (const auto &Arg : Args) {
              if (!isBlocked(Arg)) AdjustedArgs.push_back(Arg);
          }

          // B. Append our Runtime Flags
          for (const auto &Flag : RuntimeFlags) {
              // Optional: You can also filter runtime flags if needed
              if (!isBlocked(Flag)) AdjustedArgs.push_back(Flag);
          }
          
          return AdjustedArgs;
      }
  );

  return Tool.run(newFrontendActionFactory<InstrumentAction>().get());
}