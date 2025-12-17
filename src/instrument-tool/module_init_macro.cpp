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

using namespace clang;
using namespace clang::tooling;

std::vector<std::string> WnoArgs = {
      "-nostdinc",
      "-D__KERNEL__",
      "-mlittle-endian",
      "-DKASAN_SHADOW_SCALE_SHIFT=",
      "-fmacro-prefix-map=./=",
      "-Wall",
      "-Wundef",
      "-Werror=strict-prototypes",
      "-Wno-trigraphs",
      "-fno-strict-aliasing",
      "-fno-common",
      "-fshort-wchar",
      "-fno-PIE",
      "-Werror=implicit-function-declaration",
      "-Werror=implicit-int",
      "-Werror=return-type",
      "-Wno-format-security",
      "-std=gnu11",
      "-mgeneral-regs-only",
      "-DCONFIG_CC_HAS_K_CONSTRAINT=1",
      "-Wno-psabi",
      // "-mabi=lp64",
      "-fno-asynchronous-unwind-tables",
      "-fno-unwind-tables",
      "-mbranch-protection=pac-ret+leaf",
      "-Wa,-march=armv8.5-a",
      "-DARM64_ASM_ARCH=\"armv8.5-a\"",
      "-DKASAN_SHADOW_SCALE_SHIFT=",
      "-fno-delete-null-pointer-checks",
      "-Wno-frame-address",
      "-Wno-format-truncation",
      "-Wno-format-overflow",
      "-Wno-address-of-packed-member",
      "-O2",
      // "-fno-allow-store-data-races",
      "-Wframe-larger-than=2048",
      "-fstack-protector-strong",
      "-Wno-main",
      "-Wno-unused-but-set-variable",
      "-Wno-unused-const-variable",
      "-Wno-dangling-pointer",
      "-fno-omit-frame-pointer",
      "-fno-optimize-sibling-calls",
      "-ftrivial-auto-var-init=zero",
      "-fno-stack-clash-protection",
      "-Wdeclaration-after-statement",
      "-Wvla",
      "-Wno-pointer-sign",
      "-Wcast-function-type",
      "-Wno-stringop-truncation",
      "-Wno-stringop-overflow",
      "-Wno-restrict",
      "-Wno-maybe-uninitialized",
      "-Wno-array-bounds",
      "-Wno-alloc-size-larger-than",
      "-Wimplicit-fallthrough=5",
      "-fno-strict-overflow",
      "-fno-stack-check",
      // "-fconserve-stack",
      "-Werror=date-time",
      "-Werror=incompatible-pointer-types",
      "-Werror=designated-init",
      "-Wno-packed-not-aligned",
      "-g",
      "-fno-var-tracking",
      // "-femit-struct-debug-baseonly",
      "-mstack-protector-guard=sysreg",
      "-mstack-protector-guard-reg=sp_el0",
      "-mstack-protector-guard-offset=1168",
      "-DMODULE",
      "-DKBUILD_BASENAME=\"mac\"",
      "-DKBUILD_MODNAME=\"ath10k_core\"",
      "-D__KBUILD_MODNAME=kmod_ath10k_core",
      "-c",
      "-Wno-address-of-packed-member",
      "-Wno-alloc-size-larger-than",
      "-Wno-array-bounds",
      "-Wno-dangling-pointer",
      "-Wno-format-overflow",
      "-Wno-format-security",
      "-Wno-format-truncation",
      "-Wno-frame-address",
      "-Wno-main",
      "-Wno-maybe-uninitialized",
      "-Wno-missing-field-initializers",
      "-Wno-packed-not-aligned",
      "-Wno-pointer-sign",
      "-Wno-psabi",
      "-Wno-restrict",
      "-Wno-shift-negative-value",
      "-Wno-sign-compare",
      "-Wno-stringop-overflow",
      "-Wno-stringop-truncation",
      "-Wno-trigraphs",
      "-Wno-type-limits",
      "-Wno-unused-but-set-variable",
      "-Wno-unused-const-variable",
      "-Wno-unused-parameter"
};

Rewriter TheRewriter;

// 我們要插入的宣告 (告訴編譯器這個函式在別的地方)
const char *ExternDecl = "\n/* Hook defined in other file */\nvoid create_test_debugfs(void);\n\n";

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
    // 將修改後的結果輸出到螢幕 (Stdout)
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  }
};

static llvm::cl::OptionCategory MyToolCategory("instrumenter options");
int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) return 1;
  ClangTool Tool(ExpectedParser.get().getCompilations(), ExpectedParser.get().getSourcePathList());

  // 忽略錯誤，強制繼續
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-ferror-limit=0", ArgumentInsertPosition::BEGIN));
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w", ArgumentInsertPosition::BEGIN));
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=implicit-function-declaration", ArgumentInsertPosition::BEGIN));

//     // Also useful: allow int return type by default (for older C behavior)
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=strict-prototypes", ArgumentInsertPosition::END));
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=frame-address", ArgumentInsertPosition::END));
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=unused-but-set-variable", ArgumentInsertPosition::END));
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=unused-const-variable", ArgumentInsertPosition::END));
//   Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-error=no", ArgumentInsertPosition::END));
//   
  Tool.appendArgumentsAdjuster(
      [](const CommandLineArguments &Args, StringRef Filename) {
          CommandLineArguments AdjustedArgs = Args;
          
          // Append our GoldenArgs to the command line
          // Note: Standard Adjusters usually insert at beginning or end.
          // We just append everything from GoldenArgs to the existing command.
          for (const auto &Arg : WnoArgs) {
              AdjustedArgs.push_back(Arg);
          }
          return AdjustedArgs;
      }
  );
  
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("--target=aarch64-linux-gnu", ArgumentInsertPosition::BEGIN));


  return Tool.run(newFrontendActionFactory<InstrumentAction>().get());
}