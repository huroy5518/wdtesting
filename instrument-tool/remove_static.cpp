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

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

// CLI option category
static llvm::cl::OptionCategory ToolCategory("remove-static-options");

class StaticRemoverVisitor : public RecursiveASTVisitor<StaticRemoverVisitor> {
private:
    ASTContext *Context;
    Rewriter &TheRewriter;

public:
    StaticRemoverVisitor(ASTContext *Context, Rewriter &R)
        : Context(Context), TheRewriter(R) {}

    // Visit Function Declarations (includes C++ methods)
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD->isStatic()) {
            RemoveStaticKeyword(FD);
        }
        return true;
    }

    // Visit Variable Declarations (includes globals, locals, and static members)
    bool VisitVarDecl(VarDecl *VD) {
        if (VD->isStaticLocal() || VD->getStorageClass() == SC_Static) {
            RemoveStaticKeyword(VD);
        }
        return true;
    }

    // Helper to perform the actual text removal
    void RemoveStaticKeyword(Decl *D) {
        SourceManager &SM = Context->getSourceManager();
        
        // 1. Safety check: Only modify the main file, not system headers
        if (!SM.isInMainFile(D->getLocation())) return;

        // 2. Define the search range. 
        // We search from the start of the declaration up to the variable/function name.
        // This prevents us from scanning into the function body or past the variable identifier.
        SourceLocation StartLoc = D->getBeginLoc();
        SourceLocation EndLoc = D->getLocation(); // Location of the identifier

        // 3. Tokenize (Lex) the range to find "static"
        LangOptions LangOpts = Context->getLangOpts();
        SourceLocation CurrentLoc = StartLoc;
        
        while (CurrentLoc < EndLoc) {
            Token Tok;
            // Get raw token from source
            bool Failed = Lexer::getRawToken(CurrentLoc, Tok, SM, LangOpts, true);
            if (Failed) break;

            if (Tok.is(tok::kw_static)) {
                // Found it! Remove the text.
                TheRewriter.RemoveText(CurrentLoc, Tok.getLength());
                
                // Optional: Attempt to remove the space following 'static' to keep formatting clean
                // Check if the next char is a space and remove it too if desired.
                break; // Stop after finding the first static keyword for this decl
            }

            // Move to next token
            CurrentLoc = Tok.getEndLoc();
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
    virtual void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        llvm::errs() << "** Outputting processed file to stdout **\n";
        TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                           StringRef file) override {
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

    return Tool.run(newFrontendActionFactory<StaticRemoverAction>().get());
}