#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h> // Added for path manipulation
#include <llvm/Support/raw_ostream.h>

#include <sstream>
#include <stack>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <mutex>

using namespace clang;
using namespace clang::tooling;
using namespace clang::driver;
using namespace llvm;

static llvm::cl::OptionCategory InstrumentCategory("instrument-options");
static llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("Specify output filename"), llvm::cl::cat(InstrumentCategory));
static llvm::cl::opt<std::string> FlagsFile("flags-file", llvm::cl::desc("File containing build flags"), llvm::cl::cat(InstrumentCategory));

static const std::string ID_COUNTER_FILE = "_wdtest_id_counter.txt";
static const std::string INFO_LOG_FILE = "_wdtest_gen_info.txt";
static std::mutex FileMutex; 

// --- Helper: Persistent Counter ---
int fetchAndIncrementID() {
    std::lock_guard<std::mutex> lock(FileMutex);
    int current_id = 0;
    std::ifstream in_file(ID_COUNTER_FILE);
    if (in_file.is_open()) { in_file >> current_id; in_file.close(); }

    std::ofstream out_file(ID_COUNTER_FILE);
    if (out_file.is_open()) { out_file << (current_id + 1); out_file.close(); }
    return current_id;
}

class InstrumentationVisitor : public RecursiveASTVisitor<InstrumentationVisitor> {
public:
    struct BlockInfo {
        std::string block_name;
        int block_id;
        int start_line;
        int end_line;
        std::string filename;
    };
    
    // Initialize with empty filename, set it later
    explicit InstrumentationVisitor(Rewriter &R) 
        : TheRewriter(R), CurrentFilename("") {}

    // Method to set filename before traversal starts
    void setFilename(std::string F) {
        CurrentFilename = F;
    }

    ~InstrumentationVisitor() {
        if (BlockRegistry.size()) {
            std::lock_guard<std::mutex> lock(FileMutex);
            std::ofstream outputFile(INFO_LOG_FILE, std::ios::app);
            outputFile.seekp(0, std::ios::end);
            if (outputFile.tellp() == 0) {
                outputFile << "Block ID, Name, Start Line, End Line, Filename\n";
            }
            for (const auto &entry : BlockRegistry) {
                const BlockInfo &info = entry.second;
                outputFile << info.block_id << ", " << info.block_name << ", " 
                           << info.start_line << ", " << info.end_line << ", "
                           << info.filename << std::endl;
            }
        }
    }

    std::vector<std::pair<std::string, int>> ScopeStack;

    bool isNodeInMainFile(SourceLocation Loc) {
        return TheRewriter.getSourceMgr().isInMainFile(Loc);
    }

    void InstrumentBlockStart(CompoundStmt *CS, StringRef Name, int GivenID) {
        SourceLocation StartLoc = CS->getLBracLoc().getLocWithOffset(1);
        int StartLine = TheRewriter.getSourceMgr().getPresumedLoc(StartLoc).getLine();
        int EndLine = TheRewriter.getSourceMgr().getPresumedLoc(CS->getRBracLoc()).getLine();
        
        std::cout << "[File: " << CurrentFilename << "] Instrumenting '" << Name.str() << "' ID: " << GivenID << "\n";
        BlockRegistry[GivenID] = {Name.str(), GivenID, StartLine, EndLine, CurrentFilename};
        
        std::stringstream SS;
        SS << "\n    beginning_func(" << GivenID << ");";
        TheRewriter.InsertText(StartLoc, SS.str(), true, true);
    }

    void InstrumentBlockEnd(CompoundStmt *CS) {
        SourceLocation EndLoc = CS->getRBracLoc();
        if(!ScopeStack.empty()){
            auto &x = ScopeStack.back();
            std::stringstream SS;
            SS << "\n   end_func(" << x.second << ");\n";
            TheRewriter.InsertText(EndLoc, SS.str(), true, true);
        }
    }

    // --- Traversal Overrides ---
    bool TraverseFunctionDecl(FunctionDecl *FD) {
        if (!isNodeInMainFile(FD->getLocation())) return RecursiveASTVisitor::TraverseFunctionDecl(FD);

        if (FD->doesThisDeclarationHaveABody()) {
            std::string Name = FD->getNameInfo().getName().getAsString();
            int CurrentID = fetchAndIncrementID();
            ScopeStack.push_back({Name, CurrentID});
            
            if (CompoundStmt *CS = dyn_cast<CompoundStmt>(FD->getBody())) {
                InstrumentBlockStart(CS, Name, CurrentID);
                RecursiveASTVisitor::TraverseFunctionDecl(FD);
                InstrumentBlockEnd(CS);
            } else {
                RecursiveASTVisitor::TraverseFunctionDecl(FD);
            }
            ScopeStack.pop_back();
            return true;
        }
        return RecursiveASTVisitor::TraverseFunctionDecl(FD);
    }

    bool TraverseIfStmt(IfStmt *IS) {
        if (!isNodeInMainFile(IS->getBeginLoc())) return RecursiveASTVisitor::TraverseIfStmt(IS);

        if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(IS->getThen())) {
            int CurrentID = fetchAndIncrementID();
            ScopeStack.push_back({"if", CurrentID});
            InstrumentBlockStart(CS, "if", CurrentID);
            TraverseStmt(IS->getThen());
            InstrumentBlockEnd(CS);
            ScopeStack.pop_back();
        } else { TraverseStmt(IS->getThen()); }

        if (IS->getElse()) {
            if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(IS->getElse())) {
                int CurrentID = fetchAndIncrementID();
                ScopeStack.push_back({"else", CurrentID});
                InstrumentBlockStart(CS, "else", CurrentID);
                TraverseStmt(IS->getElse());
                InstrumentBlockEnd(CS);
                ScopeStack.pop_back();
            } else { TraverseStmt(IS->getElse()); }
        }
        TraverseStmt(IS->getCond());
        return true;
    }

    bool TraverseWhileStmt(WhileStmt *WS) {
        if (!isNodeInMainFile(WS->getBeginLoc())) return RecursiveASTVisitor::TraverseWhileStmt(WS);

        if (CompoundStmt *CS = dyn_cast_or_null<CompoundStmt>(WS->getBody())) {
            int CurrentID = fetchAndIncrementID();
            ScopeStack.push_back({"while", CurrentID});
            InstrumentBlockStart(CS, "while", CurrentID);
            TraverseStmt(WS->getBody());
            InstrumentBlockEnd(CS);
            ScopeStack.pop_back();
        } else { TraverseStmt(WS->getBody()); }
        TraverseStmt(WS->getCond());
        return true;
    }

    bool VisitReturnStmt(ReturnStmt *RS) {
        if (!isNodeInMainFile(RS->getBeginLoc())) return true;
        if (ScopeStack.empty()) return true;
        std::stringstream SS;
        auto &x = ScopeStack.back();
        SS << "\n  end_func(" << x.second << ");\n  "; 
        TheRewriter.InsertText(RS->getBeginLoc(), SS.str(), true, true);
        return true;
    }
    
private:
    Rewriter &TheRewriter;
    std::string CurrentFilename;
    std::map<int, BlockInfo> BlockRegistry;
};

class InstrumentationConsumer : public ASTConsumer {
public:
    InstrumentationConsumer(Rewriter &R) : Visitor(R) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        SourceManager &SM = Context.getSourceManager();
        
        // MODIFICATION: Get the Real Absolute Path of the Main File
        // 1. Get the FileEntry corresponding to the Main File ID
        const FileEntry *FE = SM.getFileEntryForID(SM.getMainFileID());
        
        std::string AbsolutePath;
        if (FE) {
            // 2. Get the name (might be relative)
            llvm::SmallString<256> PathBuf = FE->getName();
            
            // 3. Make it absolute
            // This requires llvm::sys::fs
            if (llvm::sys::fs::make_absolute(PathBuf) == std::error_code()) {
                // Optional: Normalize (remove . and ..)
                llvm::sys::path::remove_dots(PathBuf, /*remove_dot_dot=*/true);
                AbsolutePath = PathBuf.str().str();
            } else {
                // Fallback if FS fails
                AbsolutePath = FE->getName().str();
            }
        } else {
            AbsolutePath = "unknown_file.c";
        }

        // 4. Update the Visitor
        Visitor.setFilename(AbsolutePath);

        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    InstrumentationVisitor Visitor;
};

class InstrumentationAction : public ASTFrontendAction {
public:
    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        if (!OutputFilename.empty()) {
            std::error_code EC;
            llvm::raw_fd_ostream FileOS(OutputFilename, EC, llvm::sys::fs::OF_None); 
            if (!EC) TheRewriter.getEditBuffer(SM.getMainFileID()).write(FileOS);
        } else {
            TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
        }
    }
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        // We no longer pass the 'file' string here. We calculate it inside HandleTranslationUnit.
        return std::make_unique<InstrumentationConsumer>(TheRewriter);
    }
private:
    Rewriter TheRewriter;
};

bool isBlocked(const std::string &Arg) {
    static const std::vector<std::string> Blocklist = {
        "-fno-allow-store-data-races", "-fconserve-stack", "-femit-struct-debug-baseonly",
        "-mabi=lp64", "-fno-var-tracking-assignments"
    };
    for (const auto &Bad : Blocklist) if (Arg == Bad) return true;
    return false;
}

// Helper function to read flags from file
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

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, InstrumentCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    
    std::vector<std::string> RuntimeFlags;
    if (!FlagsFile.empty()) {
        RuntimeFlags = LoadFlagsFromFile(FlagsFile);
    }



    // Default Adjusters
    Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=implicit-function-declaration", ArgumentInsertPosition::BEGIN));
    Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=strict-prototypes", ArgumentInsertPosition::BEGIN));

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

    return Tool.run(newFrontendActionFactory<InstrumentationAction>().get());
}