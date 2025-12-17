.PHONY: all

LLVM_CONFIG:=$(shell llvm-config --cxxflags --ldflags --libs)

all: instrument mock module_init_macro remove_static expose_function generate_header

instrument: instrument.cpp
	clang++ instrument.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o instrument
	
mock: mock.cpp
	clang++ mock.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o mock

module_init_macro: module_init_macro.cpp
	clang++ module_init_macro.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o module_init_macro

remove_static: remove_static.cpp
	clang++ remove_static.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o remove_static

expose_function: expose_function.cpp
	clang++ expose_function.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o expose_function

generate_header: generate_header.cpp
	clang++ generate_header.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o generate_header
