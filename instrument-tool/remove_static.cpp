#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"
#include <fstream>
#include <vector>
#include <string>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("remove-static-options");

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

class StaticRemoverVisitor : public RecursiveASTVisitor<StaticRemoverVisitor> {
private:
    ASTContext *Context;
    Rewriter &TheRewriter;

public:
    StaticRemoverVisitor(ASTContext *Context, Rewriter &R)
        : Context(Context), TheRewriter(R) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD->isStatic() || FD->getStorageClass() == SC_Static) {
            llvm::errs() << "[DEBUG] Processing Function: " << FD->getNameAsString() << "\n";
            RemoveStaticKeyword(FD);
        }
        return true;
    }

    bool VisitVarDecl(VarDecl *VD) {
        if (VD->isStaticLocal() || VD->getStorageClass() == SC_Static) {
            llvm::errs() << "[DEBUG] Processing Variable: " << VD->getNameAsString() << "\n";
            RemoveStaticKeyword(VD);
        }
        return true;
    }

    void RemoveStaticKeyword(Decl *D) {
        SourceManager &SM = Context->getSourceManager();
        
        // 1. Get Physical Locations (ignores macros)
        SourceLocation StartLoc = SM.getFileLoc(D->getBeginLoc());
        SourceLocation EndLoc = SM.getFileLoc(D->getLocation());

        // 2. Main File Check
        if (SM.getFileID(StartLoc) != SM.getMainFileID()) {
             // llvm::errs() << "  [Skip] Not in main file\n";
             return;
        }

        // 3. ROBUST TEXT SEARCH STRATEGY
        // Instead of asking Lexer for tokens (which fails on kernel macros),
        // we grab the raw text buffer between start and the name.
        bool Invalid = false;
        const char *BufferStart = SM.getCharacterData(StartLoc, &Invalid);
        
        if (Invalid) {
            llvm::errs() << "  [FAIL] Invalid buffer access.\n";
            return;
        }

        // Calculate distance to search (from start of decl to the identifier name)
        unsigned OffsetStart = SM.getFileOffset(StartLoc);
        unsigned OffsetEnd = SM.getFileOffset(EndLoc);
        
        if (OffsetEnd <= OffsetStart) {
            llvm::errs() << "  [FAIL] End offset is before start.\n";
            return;
        }

        unsigned Length = OffsetEnd - OffsetStart;
        if (Length > 2000) Length = 2000; // Safety cap

        // Create a string view of the source code
        StringRef CodeSnippet(BufferStart, Length);

        // Find "static"
        // We look for "static" followed by a non-identifier character (space, tab, newline)
        // to avoid matching "static_variable_name"
        size_t Pos = CodeSnippet.find("static");
        
        if (Pos != StringRef::npos) {
            // Check boundaries to ensure it's a whole word
            // (Simplification: in C, static is usually at the start or surrounded by spaces)
            
            SourceLocation StaticKeywordLoc = StartLoc.getLocWithOffset(Pos);
            TheRewriter.RemoveText(StaticKeywordLoc, 6); // Remove "static" (len 6)
            
            // Optional: Remove trailing space
            if (Pos + 6 < Length && isspace(CodeSnippet[Pos + 6])) {
                 TheRewriter.RemoveText(StaticKeywordLoc.getLocWithOffset(6), 1);
            }

            llvm::errs() << "  [SUCCESS] Removed 'static' via buffer search.\n";
        } else {
            llvm::errs() << "  [FAIL] Keyword 'static' not found in raw text buffer.\n";
            llvm::errs() << "  [Snapshot] " << CodeSnippet.take_front(50) << "...\n";
        }
    }
};

class StaticRemoverConsumer : public ASTConsumer {
private:
    StaticRemoverVisitor Visitor;

public:
    StaticRemoverConsumer(ASTContext *Context, Rewriter &R)
        : Visitor(Context, R) {}

    virtual void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};

class StaticRemoverAction : public ASTFrontendAction {
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
            NewFilename = "output.c"; 
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
        return std::make_unique<StaticRemoverConsumer>(&CI.getASTContext(), TheRewriter);
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

    return Tool.run(newFrontendActionFactory<StaticRemoverAction>().get());
}