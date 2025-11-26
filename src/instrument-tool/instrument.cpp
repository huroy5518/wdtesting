#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <sstream>
#include <stack>
#include <iostream>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace clang::driver;
using namespace llvm;

// Command line options
static llvm::cl::OptionCategory InstrumentCategory("instrument-options");

class InstrumentationVisitor : public RecursiveASTVisitor<InstrumentationVisitor> {
public:
    explicit InstrumentationVisitor(Rewriter &R) : TheRewriter(R) {}

    // Track the names of active blocks to determine how many end_func() calls to generate on return
    std::vector<std::string> ScopeStack;

    // Helper to inject code at the start of a block
    void InstrumentBlockStart(CompoundStmt *CS, StringRef Name) {
        SourceLocation StartLoc = CS->getLBracLoc().getLocWithOffset(1);
        std::stringstream SS;
        SS << "\n    beginning_func(\"" << Name.str() << "\");";
        TheRewriter.InsertText(StartLoc, SS.str(), true, true);
    }

    // Helper to inject code at the end of a block
    void InstrumentBlockEnd(CompoundStmt *CS) {
        // std::cout << CS->getLBracLoc() << std::endl;
        // auto mgr = TheRewriter.getSourceMgr();
        SourceManager &SM = TheRewriter.getSourceMgr();
        // Get the generic SourceLocation of the function name
        SourceLocation Loc = CS->getLBracLoc();

        // Convert SourceLocation to a PresumedLoc to get line/column
        // This handles macros and #line directives correctly
        PresumedLoc PLoc = SM.getPresumedLoc(Loc);
        llvm::outs() << PLoc.getFilename() << "\n" << PLoc.getLine() << "\n";
        if (return_state.size()) {
            return_state.pop_back();
            return;
        }
        SourceLocation EndLoc = CS->getRBracLoc();
        TheRewriter.InsertText(EndLoc, "\n    end_func();\n", true, true);
    }

    // --- Traversal Overrides ---

    // 1. Functions
    bool TraverseFunctionDecl(FunctionDecl *FD) {
        // Only instrument definitions (functions with bodies)
        if (FD->doesThisDeclarationHaveABody()) {
            std::string Name = FD->getNameInfo().getName().getAsString();
            
            // Push scope
            ScopeStack.push_back(Name);
            
            // Instrument the body if it's a compound statement
            if (CompoundStmt *CS = dyn_cast<CompoundStmt>(FD->getBody())) {
                InstrumentBlockStart(CS, Name);
                
                // Traverse children (process body)
                RecursiveASTVisitor::TraverseFunctionDecl(FD);
                
                InstrumentBlockEnd(CS);
            } else {
                // Handle edge case: function body not a compound statement (rare in C functions)
                RecursiveASTVisitor::TraverseFunctionDecl(FD);
            }

            // Pop scope
            ScopeStack.pop_back();
            return true;
        }
        return RecursiveASTVisitor::TraverseFunctionDecl(FD);
    }

    // 2. If Statements
    bool TraverseIfStmt(IfStmt *IS) {
        // We only instrument the "Then" and "Else" blocks if they are CompoundStmts (have braces)
        // because the Python script logic relied on finding '{'
        
        // llvm::outs << IS->dumps() << "\n";
        bool Res = true;
        
        // Handle "Then" block
        if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(IS->getThen())) {
            llvm::outs() << "Before if\n";
            ScopeStack.push_back("if");
            InstrumentBlockStart(CS, "if");
            
            // Manually traverse the Then block to maintain scope state
            TraverseStmt(IS->getThen());
            
            InstrumentBlockEnd(CS);
            llvm::outs() << "After if\n";
            ScopeStack.pop_back();
        } else {
            // Traverse without instrumentation (single line if)
             TraverseStmt(IS->getThen());
        }

        // Handle "Else" block
        if (IS->getElse()) {
            if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(IS->getElse())) {
                ScopeStack.push_back("else");
                InstrumentBlockStart(CS, "else");
                TraverseStmt(IS->getElse());
                InstrumentBlockEnd(CS);
                ScopeStack.pop_back();
            } else {
                TraverseStmt(IS->getElse());
            }
        }
        
        // We manually traversed children, so don't call default TraverseIfStmt for children we already visited.
        // But we need to traverse the Condition.
        TraverseStmt(IS->getCond());
        
        return true;
    }

    // 3. Loops (For, While, Do) - Simplified example for While
    bool TraverseWhileStmt(WhileStmt *WS) {
        if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(WS->getBody())) {
            ScopeStack.push_back("while");
            InstrumentBlockStart(CS, "while");
            TraverseStmt(WS->getBody());
            InstrumentBlockEnd(CS);
            ScopeStack.pop_back();
        } else {
            TraverseStmt(WS->getBody());
        }
        TraverseStmt(WS->getCond());
        return true;
    }

    // (Similar logic applies for ForStmt, DoStmt, SwitchStmt...)

    // 4. Return Statements
    bool VisitReturnStmt(ReturnStmt *RS) {
        if (ScopeStack.empty()) return true;

        // Generate unwind calls based on current depth
        std::stringstream SS;
        // SS << "{ ";
        // Unwind in reverse order of the stack
        if (ScopeStack.size()) {
            
            SS << "\n  end_func();";
            SS << "\n  "; // Indentation for the return
        }
        // for (size_t i = 0; i < ScopeStack.size(); ++i) {
        // }

        // We wrap the return statement: { end_func(); ...; return X; }
        // Note: Rewriter needs to replace the entire statement.
        // This is tricky because we need the string of the original return statement.
        // For simplicity, we just insert the block start before and block end after.
        return_state.push_back(1); TheRewriter.InsertText(RS->getBeginLoc(), SS.str(), true, true);
        // TheRewriter.InsertTextAfterToken(RS->getEndLoc(), "\n}");
        
        return true;
    }

private:
    Rewriter &TheRewriter;
    std::vector<char> return_state;
};

class InstrumentationConsumer : public ASTConsumer {
public:
    InstrumentationConsumer(Rewriter &R) : Visitor(R) {}

    // Called when the AST for the entire file is parsed
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    InstrumentationVisitor Visitor;
};

class InstrumentationAction : public ASTFrontendAction {
public:
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        llvm::errs() << "** Outputting Instrumented Code **\n";
        llvm::errs() << "** ??? **\n";
        TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        llvm::errs() << "** ??asdfasdf **\n";
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<InstrumentationConsumer>(TheRewriter);
    }

private:
    Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, InstrumentCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    return Tool.run(newFrontendActionFactory<InstrumentationAction>().get());
}