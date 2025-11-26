.PHONY: all


instrument: instrument.cpp
	clang++ instrument.cpp   -O3   $(llvm-config --cxxflags --ldflags --libs)   -Wl,--start-group   -lclangTooling   -lclangFrontend   -lclangSerialization   -lclangDriver   -lclangParse   -lclangSema   -lclangAnalysis   -lclangASTMatchers   -lclangEdit   -lclangAST   -lclangLex   -lclangBasic   -lclangSupport   -lclangAPINotes -lclangRewrite  -Wl,--end-group   -o instrumenter

all: instrument
