//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Exact reference edges for one translation unit.
//
// Prints one JSON object per declaration under the include root and one edge
// per (enclosing declaration, referenced declaration) pair. The two cases the
// language server gets wrong are handled here: a template-id use resolves to
// the specialisation, which names its primary template, and every use in the
// parsed unit is visible.

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using clang::Decl;
using clang::NamedDecl;

std::string root_option;

class Collector : public clang::RecursiveASTVisitor<Collector> {
public:
  explicit Collector(clang::ASTContext &context) : context_(context) {}

  bool TraverseDecl(Decl *declaration) {
    auto *named = llvm::dyn_cast_or_null<NamedDecl>(declaration);
    if (named == nullptr) {
      return RecursiveASTVisitor::TraverseDecl(declaration);
    }
    const Decl *key = named->getCanonicalDecl();
    record(key);
    stack_.push_back(key);
    const bool result = RecursiveASTVisitor::TraverseDecl(declaration);
    stack_.pop_back();
    return result;
  }

  bool VisitDeclRefExpr(clang::DeclRefExpr *expression) {
    use(expression->getDecl());
    return true;
  }

  bool VisitMemberExpr(clang::MemberExpr *expression) {
    use(expression->getMemberDecl());
    return true;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *expression) {
    use(expression->getConstructor());
    return true;
  }

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->hasDefinition()) {
      return true;
    }
    for (const auto &base : record->bases()) {
      if (const clang::Type *type = base.getType().getTypePtrOrNull()) {
        use(type->getAsCXXRecordDecl());
      }
    }
    if (const auto *specialization =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      use(specialization->getSpecializedTemplate());
    }
    return true;
  }

  void finish() {
    for (const auto &[declaration, node] : nodes_) {
      llvm::outs() << "{\"kind\":\"decl\",\"id\":"
                   << reinterpret_cast<std::uintptr_t>(declaration)
                   << ",\"file\":\"" << node.file << "\",\"line\":" << node.line
                   << ",\"name\":\"" << node.name << "\"}\n";
    }
    for (const auto &[enclosing, targets] : edges_) {
      for (const Decl *target : targets) {
        llvm::outs() << "{\"kind\":\"edge\",\"from\":"
                     << reinterpret_cast<std::uintptr_t>(enclosing) << ",\"to\":"
                     << reinterpret_cast<std::uintptr_t>(target) << "}\n";
      }
    }
  }

private:
  static bool reportable(const Decl *declaration) {
    // Template parameters, parameters, fields, enumerators, and namespace
    // declarations are not the thing a reader deletes.
    return !llvm::isa<clang::NamespaceDecl, clang::TemplateTypeParmDecl,
                      clang::NonTypeTemplateParmDecl,
                      clang::TemplateTemplateParmDecl, clang::ParmVarDecl,
                      clang::FieldDecl, clang::EnumConstantDecl,
                      clang::UsingDecl, clang::UsingDirectiveDecl>(declaration);
  }

  void record(const Decl *declaration) {
    if (!reportable(declaration)) {
      return;
    }
    if (nodes_.count(declaration) != 0) {
      return;
    }
    const auto &manager = context_.getSourceManager();
    const clang::SourceLocation location =
        manager.getSpellingLoc(declaration->getLocation());
    if (location.isInvalid()) {
      return;
    }
    const std::string file = manager.getFilename(location).str();
    if (file.find(root_option) == std::string::npos) {
      return;
    }
    nodes_.emplace(declaration,
                   Node{file, manager.getSpellingLineNumber(location),
                        llvm::cast<NamedDecl>(declaration)->getNameAsString()});
  }

  void use(const NamedDecl *declaration) {
    if (declaration == nullptr) {
      return;
    }
    const Decl *target = declaration->getCanonicalDecl();
    if (stack_.empty() || stack_.back() == target) {
      return;
    }
    edges_[stack_.back()].insert(target);
  }

  struct Node {
    std::string file;
    unsigned line = 0;
    std::string name;
  };

  clang::ASTContext &context_;
  std::vector<const Decl *> stack_;
  std::map<const Decl *, Node> nodes_;
  std::map<const Decl *, std::set<const Decl *>> edges_;
};

class Consumer : public clang::ASTConsumer {
public:
  explicit Consumer(clang::ASTContext &context) : context_(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    Collector collector(context);
    collector.TraverseDecl(context.getTranslationUnitDecl());
    collector.finish();
  }

private:
  clang::ASTContext &context_;
};

class Action : public clang::ASTFrontendAction {
public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &instance, llvm::StringRef) override {
    return std::make_unique<Consumer>(instance.getASTContext());
  }
};

} // namespace

int main(int argc, const char **argv) {
  llvm::cl::opt<std::string> root("root", llvm::cl::desc("include root"),
                                  llvm::cl::init("include/dingo"));
  llvm::cl::opt<std::string> directory(
      "p", llvm::cl::desc("compile commands directory"), llvm::cl::init("build"));
  llvm::cl::opt<std::string> resource_dir(
      "resource-dir", llvm::cl::desc("clang resource directory"),
      llvm::cl::init("/usr/lib/clang/22"));
  llvm::cl::opt<std::string> source("source", llvm::cl::desc("<source>"),
                                    llvm::cl::Required);
  llvm::cl::opt<bool> quiet("quiet", llvm::cl::desc("suppress diagnostics"),
                            llvm::cl::init(true));
  llvm::cl::ParseCommandLineOptions(argc, argv);
  root_option = root;

  std::string error;
  auto database =
      clang::tooling::CompilationDatabase::loadFromDirectory(directory, error);
  if (!database) {
    llvm::errs() << "cannot read compile commands: " << error << "\n";
    return 1;
  }
  (void)quiet;
  clang::tooling::ClangTool tool(*database, {source});
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
      {"-resource-dir", resource_dir}, clang::tooling::ArgumentInsertPosition::BEGIN));
  return tool.run(clang::tooling::newFrontendActionFactory<Action>().get());
}
