//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2015 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "RemoveUnreferencedDecl.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"

#include "TransformationManager.h"

using namespace clang;
using namespace std;

static const char *DescriptionMsg =
    "Remove declarations that are unreferenced with the source code. \n";

static RegisterTransformation<RemoveUnreferencedDecl>
    Trans("remove-unreferenced-decl", DescriptionMsg);

class RemoveUnreferencedDecl::PropagateVisitor
    : public RecursiveASTVisitor<PropagateVisitor> {
  using Base = RecursiveASTVisitor;

  RemoveUnreferencedDecl *ConsumerInstance;

  map<pair<SourceLocation, SourceLocation>, set<Decl *>> IndexedDeclGroups;
  vector<set<Decl *>> DeclGroups;

public:
  explicit PropagateVisitor(RemoveUnreferencedDecl *Instance)
      : ConsumerInstance(Instance) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitDecl(Decl* D) {
    if (ConsumerInstance->Context->DeclMustBeEmitted(D))
      D->setReferenced();

    IndexedDeclGroups[{D->getBeginLoc(), D->getEndLoc()}].insert(D);

    return Base::VisitDecl(D);
  }

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (auto *FD2 = FD->getTemplateInstantiationPattern())
      DeclGroups.push_back({FD, FD2});

    if (auto* FD2 = FD->getPrimaryTemplate())
      DeclGroups.push_back({FD, FD2});

    if (FD->isOutOfLine())
      if (auto* FD2 = FD->getPreviousDecl())
        DeclGroups.push_back({FD, FD2});

    return Base::VisitFunctionDecl(FD);
  }
#if 0
  bool VisitTypedefNameDecl(TypedefNameDecl *D) {
    // I don't know a way to get from a instantiated typedef/using to the
    // generic one But all share the same source location. So we use that info
    // for matching...
    IndexedDeclGroups[{D->getBeginLoc(), D->getEndLoc()}].insert(D);

    return Base::VisitTypedefNameDecl(D);
  }
#endif

  bool VisitFunctionTemplateDecl(FunctionTemplateDecl *FTD) {
    if (auto *FTD2 = FTD->getInstantiatedFromMemberTemplate())
      DeclGroups.push_back({FTD, FTD2});

    return Base::VisitFunctionTemplateDecl(FTD);
  }

  bool VisitTemplateDecl(TemplateDecl *TD) {
    if (auto *D = TD->getTemplatedDecl())
      DeclGroups.push_back({TD, D});

    return Base::VisitTemplateDecl(TD);
  }

  bool VisitTemplateSpecializationType(TemplateSpecializationType *TST) {
    if (TemplateDecl *TD = TST->getTemplateName().getAsTemplateDecl()) {
      TD->setReferenced();
    }

    return Base::VisitTemplateSpecializationType(TST);
  }

  bool setReferenced(Decl *D) {
    if (D->isReferenced())
      return false;

    D->setReferenced();
    return true;
  }

  bool propagate(set<Decl *> &Decls) {
    if (Decls.size() == 1)
      return false;

    bool Referenced = false;
    for (auto *D : Decls)
      Referenced |= D->isReferenced();
    if (!Referenced)
      return false;

    bool Changed = false;

    for (auto *D : Decls)
      Changed |= setReferenced(D);

    return Changed;
  }

  bool propagate() {
    bool Changed = false;

    for (auto &Entry : IndexedDeclGroups)
      Changed |= propagate(Entry.second);
    for (auto &Decls : DeclGroups)
      Changed |= propagate(Decls);

    return Changed;
  }

  void start(TranslationUnitDecl *TUD) {
    TraverseDecl(TUD);

    while (propagate())
      ;

#ifndef NDEBUG
    TUD->dump();
#endif
  }
};

class RemoveUnreferencedDecl::CollectionVisitor
    : public RecursiveASTVisitor<CollectionVisitor> {
  RemoveUnreferencedDecl *ConsumerInstance;

public:
  explicit CollectionVisitor(RemoveUnreferencedDecl *Instance)
      : ConsumerInstance(Instance) {}

  bool VisitDecl(Decl *D) {
    if (D->isReferenced())
      return true;
    // if (isa<RecordDecl, ClassTemplateDecl>(D))
    //  return true;
    if (!isa<FunctionDecl, TypedefNameDecl>(D))
      return true;

    auto Range = ConsumerInstance->RewriteHelper->getDeclFullSourceRange(D);
    if (Range.isInvalid() || ConsumerInstance->isInIncludedFile(Range))
      return true;

    ConsumerInstance->Candidates.push_back(D);

    return true;
  }
};

void RemoveUnreferencedDecl::HandleTranslationUnit(ASTContext &Ctx) {
  PropagateVisitor(this).start(Ctx.getTranslationUnitDecl());

  CollectionVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  ValidInstanceNum = Candidates.size();

  if (QueryInstanceOnly || !checkCounterValidity())
    return;

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  doRewriting();

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

void RemoveUnreferencedDecl::doRewriting(void) {
  if (ToCounter <= 0) {
    if (TransformationCounter >= 1 &&
        TransformationCounter <= ValidInstanceNum) {
      auto *TheDecl = Candidates[TransformationCounter - 1];
      SourceRange Range = RewriteHelper->getDeclFullSourceRange(TheDecl);

      TheRewriter.RemoveText(Range);
    }
  } else {
    for (int I = ToCounter; I >= TransformationCounter; --I) {
      SourceRange Range =
          RewriteHelper->getDeclFullSourceRange(Candidates[I - 1]);

      TheRewriter.getRangeSize(Range);
      TheRewriter.getRewrittenText(Range);
      TheRewriter.RemoveText(Range);
    }
  }
}

RemoveUnreferencedDecl::~RemoveUnreferencedDecl(void) {
  // Nothing to do
}
