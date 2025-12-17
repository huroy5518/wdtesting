#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <vector>
#include <string>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolCategory("header-generator-options");

static llvm::cl::opt<std::string> OutputFileOpt(
    "o", 
    llvm::cl::desc("Specify output header filename"),
    llvm::cl::cat(ToolCategory)
);

static llvm::cl::opt<std::string> FlagFileOpt(
    "flags-file", 
    llvm::cl::desc("Path to a text file containing compiler flags"),
    llvm::cl::cat(ToolCategory)
);

// Global Output Stream Pointer
static llvm::raw_ostream *CurrentOutputStream = &llvm::outs();

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
class HeaderGeneratorVisitor : public RecursiveASTVisitor<HeaderGeneratorVisitor> {
private:
    ASTContext *Context;

public:
    HeaderGeneratorVisitor(ASTContext *Context)
        : Context(Context) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        // 1. Only definitions
        if (!FD->isThisDeclarationADefinition() || !FD->hasBody()) return true;

        // 2. Ensure it is in the Main File
        SourceManager &SM = Context->getSourceManager();
        SourceLocation StartLoc = SM.getFileLoc(FD->getBeginLoc());
        if (SM.getFileID(StartLoc) != SM.getMainFileID()) return true;

        // 3. Skip static functions
        if (FD->isStatic()) return true;

        // 4. Skip __init and __exit functions
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

        // 5. Generate Prototype
        QualType RetType = FD->getReturnType();
        std::string RetStr = RetType.getAsString();
        std::string Name = FD->getNameAsString();

        std::string Params;
        for (unsigned i = 0; i < FD->getNumParams(); ++i) {
            ParmVarDecl *Param = FD->getParamDecl(i);
            if (i > 0) Params += ", ";
            
            std::string ParamType = Param->getType().getAsString();
            std::string ParamName = Param->getNameAsString();
            
            Params += ParamType;
            if (!ParamName.empty()) {
                Params += " " + ParamName;
            }
        }
        
        if (FD->getNumParams() == 0) {
            Params = "void";
        }

        // Write to global output stream
        *CurrentOutputStream << RetStr << " " << Name << "(" << Params << ");\n";
        
        return true;
    }
};

// --- Consumer ---
class HeaderGeneratorConsumer : public ASTConsumer {
private:
    HeaderGeneratorVisitor Visitor;

public:
    HeaderGeneratorConsumer(ASTContext *Context)
        : Visitor(Context) {}

    virtual void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};

// --- Action ---
class HeaderGeneratorAction : public ASTFrontendAction {
public:
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        return std::make_unique<HeaderGeneratorConsumer>(&CI.getASTContext());
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
    
    // Output File Logic
    std::unique_ptr<llvm::raw_fd_ostream> FileOutStream;
    
    if (!OutputFileOpt.empty()) {
        std::error_code EC;
        FileOutStream = std::make_unique<llvm::raw_fd_ostream>(
            OutputFileOpt, EC, llvm::sys::fs::OF_None
        );
        
        if (EC) {
            llvm::errs() << "Error opening output file: " << EC.message() << "\n";
            return 1;
        }
        CurrentOutputStream = FileOutStream.get();
    }
    
    int Result = Tool.run(newFrontendActionFactory<HeaderGeneratorAction>().get());
    
    if (CurrentOutputStream) {
        CurrentOutputStream->flush();
    }

    return Result;
}