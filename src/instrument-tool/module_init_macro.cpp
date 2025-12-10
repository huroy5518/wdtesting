#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ModuleInitCategory("module-init-listener");

// Listens for macro expansions and prints the argument to module_init(<arg>)
class ModuleInitListener : public PPCallbacks {
public:
    ModuleInitListener(const SourceManager &SM, const LangOptions &LangOpts)
        : SM(SM), LangOpts(LangOpts) {}

    void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD,
                      SourceRange Range, const MacroArgs *Args) override {
        const IdentifierInfo *II = MacroNameTok.getIdentifierInfo();
        if (!II || II->getName() != "module_init")
            return;

        // Grab the full invocation text, then peel out the argument.
        StringRef Invocation = Lexer::getSourceText(
            CharSourceRange::getCharRange(Range), SM, LangOpts);

        StringRef Arg = Invocation;
        size_t LParen = Invocation.find('(');
        size_t RParen = Invocation.rfind(')');
        if (LParen != StringRef::npos && RParen != StringRef::npos &&
            RParen > LParen) {
            Arg = Invocation.slice(LParen + 1, RParen).trim();
        }

        errs() << "[module_init] invocation at "
               << Range.getBegin().printToString(SM) << " -> arg: " << Arg
               << "\n";
    }

private:
    const SourceManager &SM;
    const LangOptions &LangOpts;
};

class ModuleInitAction : public ASTFrontendAction {
public:
    bool BeginSourceFileAction(CompilerInstance &CI) override {
        CI.getPreprocessor().addPPCallbacks(
            std::make_unique<ModuleInitListener>(CI.getSourceManager(),
                                                 CI.getLangOpts()));
        return true;
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef InFile) override {
        return std::make_unique<ASTConsumer>();
    }
};

int main(int argc, const char **argv) {
    auto ExpectedParser =
        CommonOptionsParser::create(argc, argv, ModuleInitCategory);
    if (!ExpectedParser) {
        errs() << ExpectedParser.takeError();
        return 1;
    }

    ClangTool Tool(ExpectedParser->getCompilations(),
                   ExpectedParser->getSourcePathList());
    return Tool.run(newFrontendActionFactory<ModuleInitAction>().get());
}
