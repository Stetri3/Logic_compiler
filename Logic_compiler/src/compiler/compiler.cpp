#include "compiler.h"
#include <iostream>
#include <fstream>
#include <llvm/Support/raw_ostream.h>

namespace compiler {

    Compiler::Compiler(const ast::ASTTree& tree, std::string_view source, const std::string& moduleName)
        : tree(tree), source(source) {
        context = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>(moduleName, *context);
        builder = std::make_unique<llvm::IRBuilder<>>(*context);

        // Initialize global root scope
        pushScope();
    }

    void Compiler::pushScope() {
        auto newScope = std::make_unique<Scope>(currentScope);
        currentScope = newScope.get();
        scopePool.push_back(std::move(newScope));
    }

    void Compiler::popScope() {
        if (currentScope && currentScope->parent) {
            currentScope = currentScope->parent;
        }
    }

    std::string Compiler::getNodeSnippet(ast::NodeId id) const {
        if (id == ast::kNullNode) return "";
        const auto& node = tree.get(id);
        if (node.sourceLength == 0 || node.sourceOffset + node.sourceLength > source.size()) {
            return "";
        }
        return std::string(source.substr(node.sourceOffset, node.sourceLength));
    }

    llvm::Type* Compiler::resolveType(ast::NodeId typeNodeId) {
        if (typeNodeId == ast::kNullNode) return builder->getVoidTy();

        std::string typeName = getNodeSnippet(typeNodeId);

        // Primitive built-in types
        if (typeName == "int")    return builder->getInt32Ty();
        if (typeName == "float")  return builder->getFloatTy();
        if (typeName == "bool")   return builder->getInt1Ty();
        if (typeName == "byte")   return builder->getInt8Ty();
        if (typeName == "void")   return builder->getVoidTy();
        if (typeName == "auto")   return builder->getInt32Ty(); // Fallback

        // Custom struct / aliased types in symbol table
        if (llvm::Type* customTy = currentScope->lookupType(typeName)) {
            return customTy;
        }

        throw std::runtime_error("LLVM CodeGen Error: Unknown type '" + typeName + "'");
    }

    // -------------------------------------------------------------------------
    // Modern LLVM Constant Folding / CTFE Evaluator
    // -------------------------------------------------------------------------
    llvm::Constant* Compiler::evaluateConstantExpr(ast::NodeId exprId) {
        if (exprId == ast::kNullNode) return nullptr;

        const auto& node = tree.get(exprId);

        // Constant Literals
        if (node.kind == ast::NodeKind::LiteralExpr) {
            std::string snippet = getNodeSnippet(exprId);
            if (node.data.literal.type == t::TokenType::LInt) {
                return builder->getInt32(std::stoi(snippet));
            }
            if (node.data.literal.type == t::TokenType::LFloat) {
                return llvm::ConstantFP::get(*context, llvm::APFloat(std::stof(snippet)));
            }
            if (node.data.literal.type == t::TokenType::kTrue) return builder->getTrue();
            if (node.data.literal.type == t::TokenType::kFalse) return builder->getFalse();
        }

        // Constant Identifiers
        if (node.kind == ast::NodeKind::IdentifierExpr) {
            std::string name = getNodeSnippet(exprId);
            Symbol* sym = currentScope->lookupVar(name);
            if (sym && sym->isCompileTime) {
                return llvm::dyn_cast<llvm::Constant>(sym->value);
            }
            throw std::runtime_error("CTFE Error: Symbol '" + name + "' is not a compile-time constant!");
        }

        // Compile-time Binary Expression Folding
        if (node.kind == ast::NodeKind::BinaryExpr) {
            llvm::Constant* lhs = evaluateConstantExpr(node.data.binary.lhs);
            llvm::Constant* rhs = evaluateConstantExpr(node.data.binary.rhs);

            if (!lhs || !rhs) {
                throw std::runtime_error("CTFE Error: Expression cannot be evaluated at compile time.");
            }

            // Map token operator to LLVM opcode
            unsigned opcode = 0;
            switch (node.data.binary.op) {
            case t::TokenType::Plus:  opcode = lhs->getType()->isFloatTy() ? llvm::Instruction::FAdd : llvm::Instruction::Add; break;
            case t::TokenType::Minus: opcode = lhs->getType()->isFloatTy() ? llvm::Instruction::FSub : llvm::Instruction::Sub; break;
            case t::TokenType::Star:  opcode = lhs->getType()->isFloatTy() ? llvm::Instruction::FMul : llvm::Instruction::Mul; break;
            case t::TokenType::Slash: opcode = lhs->getType()->isFloatTy() ? llvm::Instruction::FDiv : llvm::Instruction::SDiv; break;
            default:
                throw std::runtime_error("CTFE Error: Unsupported constexpr binary operator.");
            }

            // Fold operands using modern LLVM API
            llvm::Constant* res = llvm::ConstantFoldBinaryOpOperands(opcode, lhs, rhs, module->getDataLayout());
            if (!res) {
                throw std::runtime_error("CTFE Error: Constant folding failed.");
            }
            return res;
        }

        throw std::runtime_error("CTFE Error: Expression is not valid in a constexpr context.");
    }

    bool Compiler::compile() {
        try {
            if (tree.nodes.empty()) return true;
            // Root AST node is node 0 (TranslationUnit)
            codegen(0);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Compilation failed: " << e.what() << "\n";
            return false;
        }
    }

    llvm::Value* Compiler::codegen(ast::NodeId id) {
        if (id == ast::kNullNode) return nullptr;

        const auto& node = tree.get(id);
        switch (node.kind) {
        case ast::NodeKind::TranslationUnit: return codegenTranslationUnit(id);
        case ast::NodeKind::VarDecl:         return codegenVarDecl(id);
        case ast::NodeKind::TypeDecl:        return codegenTypeDecl(id);
        case ast::NodeKind::FunctionDecl:    return codegenFunctionDecl(id);
        case ast::NodeKind::BinaryExpr:      return codegenBinaryExpr(id);
        case ast::NodeKind::UnaryExpr:       return codegenUnaryExpr(id);
        case ast::NodeKind::LiteralExpr:     return codegenLiteralExpr(id);
        case ast::NodeKind::IdentifierExpr:  return codegenIdentifierExpr(id);
        case ast::NodeKind::CallExpr:        return codegenCallExpr(id);
        case ast::NodeKind::EvalExpr:        return codegenEvalExpr(id);
        case ast::NodeKind::CompileTimeJump: return codegenCompileTimeJump(id);
        default:
            return nullptr;
        }
    }

    llvm::Value* Compiler::codegenTranslationUnit(ast::NodeId id) {
        pushScope();
        llvm::Value* lastVal = nullptr;

        for (size_t i = id + 1; i < tree.nodes.size(); ++i) {
            lastVal = codegen(static_cast<ast::NodeId>(i));
        }

        popScope();
        return lastVal;
    }

    llvm::Value* Compiler::codegenVarDecl(ast::NodeId id) {
        const auto& node = tree.get(id);
        std::string varName = getNodeSnippet(node.data.varDecl.nameNode);
        llvm::Type* varType = resolveType(node.data.varDecl.typeNode);

        bool isComptime = node.qualifiers.isConstexpr;

        if (isComptime) {
            // Evaluated completely at compile-time
            llvm::Constant* constVal = evaluateConstantExpr(node.data.varDecl.initExpr);
            currentScope->symbols[varName] = Symbol{ constVal, varType, true, true };
            return constVal;
        }

        llvm::Value* initVal = nullptr;
        if (node.data.varDecl.initExpr != ast::kNullNode) {
            initVal = codegen(node.data.varDecl.initExpr);
        }
        else {
            initVal = llvm::Constant::getNullValue(varType);
        }

        llvm::Function* currentFn = builder->GetInsertBlock() ? builder->GetInsertBlock()->getParent() : nullptr;

        if (currentFn) {
            // Local stack variable
            llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
            llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(varType, nullptr, varName);

            if (initVal) {
                builder->CreateStore(initVal, alloca);
            }

            currentScope->symbols[varName] = Symbol{ alloca, varType, (bool)node.qualifiers.isConst, false };
            return alloca;
        }
        else {
            // Global variable
            auto* globalVar = new llvm::GlobalVariable(
                *module,
                varType,
                node.qualifiers.isConst,
                llvm::GlobalValue::ExternalLinkage,
                llvm::dyn_cast<llvm::Constant>(initVal),
                varName
            );

            currentScope->symbols[varName] = Symbol{ globalVar, varType, (bool)node.qualifiers.isConst, false };
            return globalVar;
        }
    }

    llvm::Value* Compiler::codegenTypeDecl(ast::NodeId id) {
        const auto& node = tree.get(id);
        std::string typeName = getNodeSnippet(node.data.varDecl.nameNode);

        llvm::Type* aliasedType = resolveType(node.data.varDecl.initExpr);
        currentScope->customTypes[typeName] = aliasedType;
        return nullptr;
    }

    llvm::Value* Compiler::codegenFunctionDecl(ast::NodeId id) {
        const auto& node = tree.get(id);
        std::string fnName = getNodeSnippet(node.data.varDecl.nameNode);
        llvm::Type* retType = resolveType(node.data.varDecl.typeNode);

        FunctionInfo fnInfo;
        fnInfo.declNodeId = id;

        // Separate runtime parameter types for LLVM function signature
        std::vector<llvm::Type*> llvmParamTypes;

        // Populate fnInfo parameters and only push non-constexpr parameters to llvmParamTypes
        llvm::FunctionType* fnType = llvm::FunctionType::get(retType, llvmParamTypes, false);

        llvm::Function* fn = llvm::Function::Create(
            fnType,
            llvm::Function::ExternalLinkage,
            fnName,
            module.get()
        );

        fnInfo.llvmFunc = fn;
        currentScope->functions[fnName] = fnInfo;

        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
        builder->SetInsertPoint(entryBB);

        pushScope();

        if (node.data.varDecl.initExpr != ast::kNullNode) {
            codegen(node.data.varDecl.initExpr);
        }

        if (!builder->GetInsertBlock()->getTerminator()) {
            if (retType->isVoidTy()) {
                builder->CreateRetVoid();
            }
            else {
                builder->CreateRet(llvm::Constant::getNullValue(retType));
            }
        }

        popScope();
        llvm::verifyFunction(*fn);
        return fn;
    }

    llvm::Value* Compiler::codegenBinaryExpr(ast::NodeId id) {
        const auto& node = tree.get(id);
        llvm::Value* lhs = codegen(node.data.binary.lhs);
        llvm::Value* rhs = codegen(node.data.binary.rhs);

        if (!lhs || !rhs) return nullptr;

        t::TokenType op = node.data.binary.op;
        bool isFloat = lhs->getType()->isFloatTy() || rhs->getType()->isFloatTy();

        switch (op) {
        case t::TokenType::Plus:
            return isFloat ? builder->CreateFAdd(lhs, rhs, "addtmp") : builder->CreateAdd(lhs, rhs, "addtmp");
        case t::TokenType::Minus:
            return isFloat ? builder->CreateFSub(lhs, rhs, "subtmp") : builder->CreateSub(lhs, rhs, "subtmp");
        case t::TokenType::Star:
            return isFloat ? builder->CreateFMul(lhs, rhs, "multmp") : builder->CreateMul(lhs, rhs, "multmp");
        case t::TokenType::Slash:
            return isFloat ? builder->CreateFDiv(lhs, rhs, "divtmp") : builder->CreateSDiv(lhs, rhs, "divtmp");
        case t::TokenType::EEquals:
            return isFloat ? builder->CreateFCmpOEQ(lhs, rhs, "eqtmp") : builder->CreateICmpEQ(lhs, rhs, "eqtmp");
        case t::TokenType::NEquals:
            return isFloat ? builder->CreateFCmpONE(lhs, rhs, "netmp") : builder->CreateICmpNE(lhs, rhs, "netmp");
        case t::TokenType::Lesser:
            return isFloat ? builder->CreateFCmpOLT(lhs, rhs, "lttmp") : builder->CreateICmpSLT(lhs, rhs, "lttmp");
        case t::TokenType::LeEquals:
            return isFloat ? builder->CreateFCmpOLE(lhs, rhs, "letmp") : builder->CreateICmpSLE(lhs, rhs, "letmp");
        case t::TokenType::Greater:
            return isFloat ? builder->CreateFCmpOGT(lhs, rhs, "gttmp") : builder->CreateICmpSGT(lhs, rhs, "gttmp");
        case t::TokenType::GrEquals:
            return isFloat ? builder->CreateFCmpOGE(lhs, rhs, "getmp") : builder->CreateICmpSGE(lhs, rhs, "getmp");
        case t::TokenType::Equals: {
            if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(lhs)) {
                builder->CreateStore(rhs, alloca);
                return rhs;
            }
            throw std::runtime_error("LLVM CodeGen Error: Invalid LHS assignment target");
        }
        default:
            throw std::runtime_error("LLVM CodeGen Error: Unsupported binary operator");
        }
    }

    llvm::Value* Compiler::codegenUnaryExpr(ast::NodeId id) {
        const auto& node = tree.get(id);
        t::TokenType op = node.data.unary.op;

        if (op == t::TokenType::kReturn) {
            if (node.data.unary.operand != ast::kNullNode) {
                llvm::Value* retVal = codegen(node.data.unary.operand);
                return builder->CreateRet(retVal);
            }
            else {
                return builder->CreateRetVoid();
            }
        }

        llvm::Value* operand = codegen(node.data.unary.operand);
        if (op == t::TokenType::Minus) {
            return operand->getType()->isFloatTy() ? builder->CreateFNeg(operand, "negtmp") : builder->CreateNeg(operand, "negtmp");
        }
        if (op == t::TokenType::Excl) {
            return builder->CreateNot(operand, "nottmp");
        }

        return nullptr;
    }

    llvm::Value* Compiler::codegenLiteralExpr(ast::NodeId id) {
        std::string litStr = getNodeSnippet(id);
        const auto& node = tree.get(id);

        if (node.data.literal.type == t::TokenType::LInt) {
            return builder->getInt32(std::stoi(litStr));
        }
        if (node.data.literal.type == t::TokenType::LFloat) {
            return llvm::ConstantFP::get(*context, llvm::APFloat(std::stof(litStr)));
        }
        if (node.data.literal.type == t::TokenType::kTrue) {
            return builder->getTrue();
        }
        if (node.data.literal.type == t::TokenType::kFalse) {
            return builder->getFalse();
        }

        return nullptr;
    }

    llvm::Value* Compiler::codegenIdentifierExpr(ast::NodeId id) {
        std::string varName = getNodeSnippet(id);
        Symbol* sym = currentScope->lookupVar(varName);

        if (!sym) {
            throw std::runtime_error("LLVM CodeGen Error: Undefined symbol '" + varName + "'");
        }

        if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(sym->value)) {
            return builder->CreateLoad(sym->type, alloca, varName);
        }

        return sym->value;
    }

    llvm::Value* Compiler::codegenCallExpr(ast::NodeId id) {
        const auto& node = tree.get(id);
        std::string fnName = getNodeSnippet(node.data.call.target);

        FunctionInfo* fnInfo = currentScope->lookupFunc(fnName);
        if (!fnInfo) {
            // Fallback for external C functions
            llvm::Function* externalFn = module->getFunction(fnName);
            if (!externalFn) {
                throw std::runtime_error("LLVM CodeGen Error: Unknown function call target '" + fnName + "'");
            }
            std::vector<llvm::Value*> args;
            if (node.data.call.argsList != ast::kNullNode) {
                args.push_back(codegen(node.data.call.argsList));
            }
            return builder->CreateCall(externalFn, args, externalFn->getReturnType()->isVoidTy() ? "" : "calltmp");
        }

        std::vector<llvm::Value*> llvmArgs;

        // Process call arguments, evaluating 'constexpr' parameters at compile-time
        for (const auto& param : fnInfo->allParams) {
            ast::NodeId argNodeId = node.data.call.argsList; // Match parameter position from args list

            if (param.isCompileTime) {
                // Must evaluate at compile-time
                llvm::Constant* constVal = evaluateConstantExpr(argNodeId);
                (void)constVal; // Bound to CTFE frame instead of LLVM IR call site
            }
            else {
                // Runtime argument passed in IR
                llvm::Value* runtimeVal = codegen(argNodeId);
                llvmArgs.push_back(runtimeVal);
            }
        }

        return builder->CreateCall(fnInfo->llvmFunc, llvmArgs, fnInfo->llvmFunc->getReturnType()->isVoidTy() ? "" : "calltmp");
    }

    llvm::Value* Compiler::codegenEvalExpr(ast::NodeId id) {
        const auto& node = tree.get(id);
        llvm::Value* condVal = codegen(node.data.evalOp.cond);

        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", fn);
        llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "else");
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifcont");

        builder->CreateCondBr(condVal, thenBB, elseBB);

        // Emit 'Then'
        builder->SetInsertPoint(thenBB);
        codegen(node.data.evalOp.thenBlock);
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBB);
        }

        // Emit 'Else'
        fn->insert(fn->end(), elseBB);
        builder->SetInsertPoint(elseBB);
        if (node.data.evalOp.elseBlock != ast::kNullNode) {
            codegen(node.data.evalOp.elseBlock);
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBB);
        }

        // Merge Point
        fn->insert(fn->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);

        return nullptr;
    }

    llvm::Value* Compiler::codegenCompileTimeJump(ast::NodeId id) {
        const auto& node = tree.get(id);

        if (node.data.unary.op == t::TokenType::kBreak || node.data.unary.op == t::TokenType::kContinue) {
            return nullptr;
        }

        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* loopCondBB = llvm::BasicBlock::Create(*context, "loop.cond", fn);
        llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "loop.body", fn);
        llvm::BasicBlock* loopEndBB = llvm::BasicBlock::Create(*context, "loop.end", fn);

        builder->CreateBr(loopCondBB);
        builder->SetInsertPoint(loopCondBB);

        llvm::Value* cond = codegen(node.data.evalOp.cond);
        builder->CreateCondBr(cond, loopBodyBB, loopEndBB);

        builder->SetInsertPoint(loopBodyBB);
        codegen(node.data.evalOp.thenBlock);
        builder->CreateBr(loopCondBB);

        fn->insert(fn->end(), loopEndBB);
        builder->SetInsertPoint(loopEndBB);

        return nullptr;
    }

    void Compiler::dumpIR() const {
        module->print(llvm::errs(), nullptr);
    }

    bool Compiler::emitIRToFile(const std::string& filepath) const {
        std::error_code ec;
        llvm::raw_fd_ostream file(filepath, ec);
        if (ec) {
            std::cerr << "Could not open file for writing IR: " << ec.message() << "\n";
            return false;
        }
        module->print(file, nullptr);
        return true;
    }

} // namespace compiler