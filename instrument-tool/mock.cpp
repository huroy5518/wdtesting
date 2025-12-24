#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>
#include <set>
#include <iostream>
#include <fstream>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace clang::driver;
using namespace llvm;

// --- Command Line Options ---
static llvm::cl::OptionCategory MockReplacerCategory("mock-replacer-options");

// Option: --suffix for output files (e.g., ".mocked")
// If empty, output goes to stdout.
static llvm::cl::opt<std::string> OutputSuffix("suffix", 
    llvm::cl::desc("Suffix to append to output files (e.g., '.mocked'). If omitted, prints to stdout."), 
    llvm::cl::cat(MockReplacerCategory));

// Option: --flags-file for reading build flags
static llvm::cl::opt<std::string> FlagsFile("flags-file", 
    llvm::cl::desc("File containing build flags (one per line). Note: Use -p <dir> for compile_commands.json"), 
    llvm::cl::cat(MockReplacerCategory));


struct MockRule {
    std::string ReplacementName;
    std::string ReplacementSignature; 
};

// Global Map: OriginalName -> Rule
// We use a static map so rules found in File 1 persist when processing File 2.
static std::map<std::string, MockRule> MockMap;
static std::set<std::pair<unsigned, std::string>> InjectedDeclarations;

class MockVisitor : public RecursiveASTVisitor<MockVisitor> {
public:
    explicit MockVisitor(Rewriter &R) : TheRewriter(R) {}

    std::string generateSignature(const FunctionDecl *FD) {
        std::string Sig = "extern " + FD->getReturnType().getAsString() + " " + FD->getNameAsString() + "(";
        for (unsigned i = 0; i < FD->getNumParams(); ++i) {
            if (i > 0) Sig += ", ";
            Sig += FD->getParamDecl(i)->getType().getAsString();
        }
        if (FD->isVariadic()) {
            if (FD->getNumParams() > 0) Sig += ", ";
            Sig += "...";
        }
        Sig += ");";
        return Sig;
    }

    bool VisitCallExpr(CallExpr *Call) {
        FunctionDecl *Callee = Call->getDirectCallee();
        if (!Callee) return true;

        std::string FuncName = Callee->getNameInfo().getName().getAsString();

        // ---------------------------------------------------------
        // PHASE 1: Harvesting MOCK_FUNC definitions
        // ---------------------------------------------------------
        if (FuncName == "MOCK_FUNC") {
            if (Call->getNumArgs() >= 2) {
                const Expr *Arg0 = Call->getArg(0)->IgnoreParenImpCasts();
                const Expr *Arg1 = Call->getArg(1)->IgnoreParenImpCasts();

                std::string OriginalName;
                std::string ReplacementName;
                std::string Signature;

                if (const clang::StringLiteral *SL = dyn_cast<clang::StringLiteral>(Arg0)) {
                    OriginalName = SL->getString().str();
                }

                if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Arg1)) {
                    ReplacementName = DRE->getNameInfo().getName().getAsString();
                    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
                        Signature = generateSignature(FD);
                    }
                }

                if (!OriginalName.empty() && !ReplacementName.empty()) {
                    MockRule NewRule = {ReplacementName, Signature};
                    
                    // Update Memory Map
                    MockMap[OriginalName] = NewRule;

                    llvm::outs() << "[Config] Captured Rule: " << OriginalName 
                                 << " -> " << ReplacementName << "\n";
                    
                    // --- DELETION LOGIC ---
                    SourceRange RemovalRange = Call->getSourceRange();
                    SourceLocation CallEnd = Call->getEndLoc();
                    SourceManager &SM = TheRewriter.getSourceMgr();
                    const LangOptions &LangOpts = TheRewriter.getLangOpts();

                    std::optional<Token> NextTok = Lexer::findNextToken(CallEnd, SM, LangOpts);
                    
                    if (NextTok && NextTok->is(tok::semi)) {
                        TheRewriter.RemoveText(RemovalRange); 
                        TheRewriter.RemoveText(NextTok->getLocation(), 1); 
                    } else {
                        TheRewriter.RemoveText(RemovalRange);
                    }
                }
            }
        } 
        
        // ---------------------------------------------------------
        // PHASE 2: Refactoring Calls
        // ---------------------------------------------------------
        else {
            auto It = MockMap.find(FuncName);
            if (It != MockMap.end()) {
                MockRule &Rule = It->second;
                SourceManager &SM = TheRewriter.getSourceMgr();
                SourceLocation Loc = Call->getBeginLoc();
                FileID FID = SM.getFileID(Loc);
                
                // 1. Inject extern declaration if needed
                unsigned FileHash = FID.getHashValue();
                if (!Rule.ReplacementSignature.empty() && 
                    InjectedDeclarations.find({FileHash, Rule.ReplacementName}) == InjectedDeclarations.end()) {
                    
                    TheRewriter.InsertText(SM.getLocForStartOfFile(FID), "\n" + Rule.ReplacementSignature + "\n");
                    InjectedDeclarations.insert({FileHash, Rule.ReplacementName});
                }

                // 2. Replace the function call
                std::string NewName = Rule.ReplacementName;
                SourceLocation StartLoc = Call->getCallee()->getBeginLoc();
                
                TheRewriter.ReplaceText(StartLoc, FuncName.length(), NewName);
                
                auto FileRef = SM.getFileEntryRefForID(FID);
                llvm::outs() << "[Refactor] Replaced " << FuncName 
                             << " in " << (FileRef ? FileRef->getName() : "unknown") << "\n";
            }
        }

        return true;
    }

private:
    Rewriter &TheRewriter;
};

class MockConsumer : public ASTConsumer {
public:
    MockConsumer(Rewriter &R) : Visitor(R) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    MockVisitor Visitor;
};

class MockAction : public ASTFrontendAction {
public:
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        FileID ID = SM.getMainFileID();
        if (ID.isInvalid()) return;
        
        // Output Logic:
        // 1. If --suffix is provided, write to <Filename><Suffix>
        // 2. If no suffix, write to stdout
        
        bool HasChanges = TheRewriter.getEditBuffer(ID).size() > 0;
        
        if (!OutputSuffix.empty()) {
            const FileEntry *FE = SM.getFileEntryForID(ID);
            if (FE) {
                std::string NewFilename = std::string(FE->getName()) + OutputSuffix;
                
                std::error_code EC;
                llvm::raw_fd_ostream FileOS(NewFilename, EC, llvm::sys::fs::OF_None);
                if (EC) {
                    llvm::errs() << "Error opening output file " << NewFilename << ": " << EC.message() << "\n";
                    return;
                }
                // Write modified buffer (or original if no changes, usually desired to keep file complete)
                TheRewriter.getEditBuffer(ID).write(FileOS);
                llvm::outs() << ">> Wrote output to " << NewFilename << "\n";
            }
        } else {
            // Fallback to stdout
            if (HasChanges) {
                auto FileRef = SM.getFileEntryRefForID(ID);
                llvm::outs() << ">>>>> Modified Source for " 
                             << (FileRef ? FileRef->getName() : "unknown") << " <<<<<\n";
                TheRewriter.getEditBuffer(ID).write(llvm::outs());
            }
        }
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MockConsumer>(TheRewriter);
    }

private:
    Rewriter TheRewriter;
};

// Helper function to read flags from file
std::vector<std::string> readFlagsFromFile(const std::string& filename) {
    std::vector<std::string> flags;
    std::ifstream file(filename);
    std::string word;
    if (file.is_open()) {
        while (file >> word) {
            flags.push_back(word);
        }
        file.close();
    } else {
        llvm::errs() << "Warning: Could not open flags file: " << filename << "\n";
    }
    return flags;
}

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MockReplacerCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    // 1. Read flags from --flags-file if provided
    if (!FlagsFile.empty()) {
        std::vector<std::string> extraFlags = readFlagsFromFile(FlagsFile);
        for (const auto& flag : extraFlags) {
            Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(flag.c_str(), ArgumentInsertPosition::BEGIN));
        }
        llvm::outs() << "Loaded " << extraFlags.size() << " flags from " << FlagsFile << "\n";
    }

    return Tool.run(newFrontendActionFactory<MockAction>().get());
}