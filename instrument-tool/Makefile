.PHONY: all

LLVM_CONFIG:=$(shell llvm-config --cxxflags --ldflags --libs)

all: instrument mock module_init_macro remove_static

instrument: instrument.cpp
	clang++ instrument.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o instrument
	
mock: mock.cpp
	clang++ mock.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o mock

module_init_macro: module_init_macro.cpp
	clang++ module_init_macro.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o module_init_macro

remove_static: remove_static.cpp
	clang++ remove_static.cpp   -O3 $(LLVM_CONFIG)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o remove_static

