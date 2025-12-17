#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Lexer.h"
#include <map>
#include <string>
#include <set>
#include <iostream>

using namespace clang;
using namespace clang::tooling;
using namespace clang::driver;
using namespace llvm;

static llvm::cl::OptionCategory MockReplacerCategory("mock-replacer-options");

struct MockRule {
    std::string ReplacementName;
    std::string ReplacementSignature; 
};

// Global Map: OriginalName -> Rule
// Removed persistence/database logic to simplify back to 2-parameter in-memory version.
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
            // Reverted to 2 arguments: MOCK_FUNC("original", replacement)
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
                // (This ensures the replacement function is known in the current file)
                unsigned FileHash = FID.getHashValue();
                if (!Rule.ReplacementSignature.empty() && 
                    InjectedDeclarations.find({FileHash, Rule.ReplacementName}) == InjectedDeclarations.end()) {
                    
                    TheRewriter.InsertText(SM.getLocForStartOfFile(FID), "\n" + Rule.ReplacementSignature + "\n");
                    InjectedDeclarations.insert({FileHash, Rule.ReplacementName});
                }

                // 2. Replace the function call
                std::string NewName = Rule.ReplacementName;
                SourceLocation StartLoc = Call->getCallee()->getBeginLoc();
                
                // Simple replacement without file path checks
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
        
        if (TheRewriter.getEditBuffer(ID).size() > 0) {
             auto FileRef = SM.getFileEntryRefForID(ID);
             llvm::outs() << ">>>>> Modified Source for " 
                          << (FileRef ? FileRef->getName() : "unknown") << " <<<<<\n";
             TheRewriter.getEditBuffer(ID).write(llvm::outs());
        }
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MockConsumer>(TheRewriter);
    }

private:
    Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MockReplacerCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<MockAction>().get());
}