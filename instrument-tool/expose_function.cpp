#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <vector>
#include <string>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("add-export-options");

static llvm::cl::opt<std::string> OutputFileOpt(
    "o", 
    llvm::cl::desc("Specify output filename"),
    llvm::cl::cat(ToolCategory)
);

static llvm::cl::opt<std::string> FlagFileOpt(
    "flags-file", 
    llvm::cl::desc("Path to a text file containing compiler flags"),
    llvm::cl::cat(ToolCategory)
);

// --- Helper Functions ---
std::vector<std::string> LoadFlagsFromFile(const std::string &FilePath) {
    std::vector<std::string> Flags;
    std::ifstream File(FilePath);
    if (!File.is_open()) return Flags;
    std::string Line;
    while (std::getline(File, Line)) {
        if (Line.empty() || Line[0] == '#') continue;
        size_t first = Line.find_first_not_of(" \t\r\n");
        size_t last = Line.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        Flags.push_back(Line.substr(first, (last - first + 1)));
    }
    return Flags;
}

bool isBlocked(const std::string &Arg) {
    static const std::vector<std::string> Blocklist = {
        "-fno-allow-store-data-races", "-fconserve-stack", "-femit-struct-debug-baseonly",
        "-mabi=lp64", "-fno-var-tracking-assignments"
    };
    for (const auto &Bad : Blocklist) if (Arg == Bad) return true;
    return false;
}

// --- Visitor ---
class ExportVisitor : public RecursiveASTVisitor<ExportVisitor> {
private:
    ASTContext *Context;
    Rewriter &TheRewriter;

public:
    ExportVisitor(ASTContext *Context, Rewriter &R)
        : Context(Context), TheRewriter(R) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        // 1. Must be a definition (have a body)
        if (!FD->isThisDeclarationADefinition() || !FD->hasBody()) return true;

        SourceManager &SM = Context->getSourceManager();
        SourceLocation StartLoc = SM.getFileLoc(FD->getBeginLoc());
        
        // 2. Must be in Main File
        if (SM.getFileID(StartLoc) != SM.getMainFileID()) return true;

        // 3. Skip Static functions (safeguard)
        if (FD->isStatic()) return true;

        // 4. Skip __init and __exit functions
        // We check the raw source text for these macros
        SourceLocation NameLoc = FD->getLocation();
        unsigned StartOff = SM.getFileOffset(StartLoc);
        unsigned EndOff = SM.getFileOffset(NameLoc);
        
        bool Invalid = false;
        const char *BufStart = SM.getCharacterData(StartLoc, &Invalid);
        
        if (!Invalid && EndOff > StartOff) {
            StringRef DeclText(BufStart, EndOff - StartOff);
            if (DeclText.contains("__init") || DeclText.contains("__exit")) {
                llvm::errs() << "[Skip] Init/Exit function: " << FD->getNameAsString() << "\n";
                return true;
            }
        }

        // 5. Insert EXPORT_SYMBOL
        std::string FuncName = FD->getNameAsString();
        Stmt *Body = FD->getBody();
        if (!Body) return true;

        SourceLocation EndBodyLoc = Body->getEndLoc(); // Points to '}'
        
        // Insert AFTER the closing brace
        std::string ExportMacro = "\nEXPORT_SYMBOL(" + FuncName + ");\n";
        TheRewriter.InsertTextAfterToken(EndBodyLoc, ExportMacro);

        llvm::errs() << "[+] Added EXPORT_SYMBOL for: " << FuncName << "\n";
        
        return true;
    }
};

// --- Consumer ---
class ExportConsumer : public ASTConsumer {
private:
    ExportVisitor Visitor;

public:
    ExportConsumer(ASTContext *Context, Rewriter &R)
        : Visitor(Context, R) {}

    virtual void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};

// --- Action ---
class AddExportAction : public ASTFrontendAction {
private:
    Rewriter TheRewriter;

public:
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        FileID MainFileID = SM.getMainFileID();

        std::string NewFilename;
        if (!OutputFileOpt.empty()) {
            NewFilename = OutputFileOpt;
        } else {
            NewFilename = "output_exported.c"; 
        }

        std::error_code EC;
        llvm::raw_fd_ostream OutFile(NewFilename, EC, llvm::sys::fs::OF_None);
        if (EC) {
            llvm::errs() << "[!] Error writing: " << EC.message() << "\n";
            return;
        }

        TheRewriter.getEditBuffer(MainFileID).write(OutFile);
        llvm::outs() << "[*] Saved: " << NewFilename << "\n";
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<ExportConsumer>(&CI.getASTContext(), TheRewriter);
    }
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    
    std::vector<std::string> RuntimeFlags;
    if (!FlagFileOpt.empty()) {
        RuntimeFlags = LoadFlagsFromFile(FlagFileOpt);
    }

    Tool.appendArgumentsAdjuster(
        [&](const CommandLineArguments &Args, StringRef Filename) {
            CommandLineArguments AdjustedArgs;
            for (const auto &Arg : Args) {
                if (!isBlocked(Arg)) AdjustedArgs.push_back(Arg);
            }
            for (const auto &Flag : RuntimeFlags) {
                if (!isBlocked(Flag)) AdjustedArgs.push_back(Flag);
            }
            AdjustedArgs.push_back("-x");
            AdjustedArgs.push_back("c");
            return AdjustedArgs;
        }
    );

    return Tool.run(newFrontendActionFactory<AddExportAction>().get());
}