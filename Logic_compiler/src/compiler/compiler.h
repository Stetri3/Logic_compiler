#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string_view>

// LLVM Includes
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/Analysis/ConstantFolding.h>

#include "AST_def.h"
#include "token.h"

namespace compiler {

    struct ParamInfo {
        std::string name;
        llvm::Type* type{ nullptr };
        bool isCompileTime{ false }; // 'constexpr' parameter
    };

    struct FunctionInfo {
        llvm::Function* llvmFunc{ nullptr };
        ast::NodeId declNodeId{ ast::kNullNode };
        std::vector<ParamInfo> allParams;       // Full param list (runtime + constexpr)
        std::vector<ParamInfo> runtimeParams;   // Only runtime params present in LLVM signature
    };

    struct Symbol {
        llvm::Value* value{ nullptr }; // AllocaInst* for mutable vars or Constant* for constexpr
        llvm::Type* type{ nullptr };
        bool isConst{ false };
        bool isCompileTime{ false };   // True if value is a known compile-time constant
    };

    class Scope {
    public:
        std::unordered_map<std::string, Symbol> symbols;
        std::unordered_map<std::string, llvm::Type*> customTypes;
        std::unordered_map<std::string, FunctionInfo> functions;
        Scope* parent{ nullptr };

        explicit Scope(Scope* parent = nullptr) : parent(parent) {}

        Symbol* lookupVar(const std::string& name) {
            auto it = symbols.find(name);
            if (it != symbols.end()) return &it->second;
            if (parent) return parent->lookupVar(name);
            return nullptr;
        }

        FunctionInfo* lookupFunc(const std::string& name) {
            auto it = functions.find(name);
            if (it != functions.end()) return &it->second;
            if (parent) return parent->lookupFunc(name);
            return nullptr;
        }

        llvm::Type* lookupType(const std::string& name) {
            auto it = customTypes.find(name);
            if (it != customTypes.end()) return it->second;
            if (parent) return parent->lookupType(name);
            return nullptr;
        }
    };

    class Compiler {
    public:
        Compiler(const ast::ASTTree& tree, std::string_view source, const std::string& moduleName = "logic_module");
        ~Compiler() = default;

        // Main CodeGen entry point
        bool compile();

        // Output IR
        void dumpIR() const;
        bool emitIRToFile(const std::string& filepath) const;

        llvm::Module* getModule() { return module.get(); }

    private:
        const ast::ASTTree& tree;
        std::string_view source;

        // LLVM Core
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::IRBuilder<>> builder;

        // Scope Management
        Scope* currentScope{ nullptr };
        std::vector<std::unique_ptr<Scope>> scopePool;

        void pushScope();
        void popScope();

        // Helpers
        std::string getNodeSnippet(ast::NodeId id) const;
        llvm::Type* resolveType(ast::NodeId typeNodeId);

        // Compile-Time Evaluator (CTFE for 'constexpr' params and expressions)
        llvm::Constant* evaluateConstantExpr(ast::NodeId exprId);

        // Core CodeGen Visiting Methods
        llvm::Value* codegen(ast::NodeId id);
        llvm::Value* codegenTranslationUnit(ast::NodeId id);
        llvm::Value* codegenVarDecl(ast::NodeId id);
        llvm::Value* codegenTypeDecl(ast::NodeId id);
        llvm::Value* codegenFunctionDecl(ast::NodeId id);
        llvm::Value* codegenBinaryExpr(ast::NodeId id);
        llvm::Value* codegenUnaryExpr(ast::NodeId id);
        llvm::Value* codegenLiteralExpr(ast::NodeId id);
        llvm::Value* codegenIdentifierExpr(ast::NodeId id);
        llvm::Value* codegenCallExpr(ast::NodeId id);
        llvm::Value* codegenEvalExpr(ast::NodeId id);
        llvm::Value* codegenCompileTimeJump(ast::NodeId id);
    };

} // namespace compiler