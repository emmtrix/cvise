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
#  include <config.h>
#endif

#include "RemoveUnreferencedDecl.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"

#include "TransformationManager.h"

using namespace clang;

static const char *DescriptionMsg =
"Remove declarations that are unreferenced with the source code. \n";

static RegisterTransformation<RemoveUnreferencedDecl>
         Trans("remove-unreferenced-decl", DescriptionMsg);


class RemoveUnreferencedDecl::PropagateVisitor
    : public RecursiveASTVisitor<PropagateVisitor> {
  RemoveUnreferencedDecl *ConsumerInstance;

public:
  explicit PropagateVisitor(RemoveUnreferencedDecl *Instance)
      : ConsumerInstance(Instance) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitFunctionDecl(FunctionDecl *D) {
    if (!D->isReferenced())
      return true;

    if (auto *FD2 = D->getTemplateInstantiationPattern())
      FD2->setReferenced();

    return true;
  }

  bool VisitTemplateSpecializationType(TemplateSpecializationType* TST) {
    if (TemplateDecl *TD = TST->getTemplateName().getAsTemplateDecl()) {
      TD->setReferenced();
      TD->getTemplatedDecl()->setReferenced();
    }

    return true;
  }
};

class RemoveUnreferencedDecl::CollectionVisitor : public RecursiveASTVisitor<CollectionVisitor> {
    RemoveUnreferencedDecl* ConsumerInstance;

public:
  explicit CollectionVisitor(RemoveUnreferencedDecl* Instance) : ConsumerInstance(Instance)
  { }

  bool VisitDecl(Decl* D) {
    if (D->isReferenced() || ConsumerInstance->Context->DeclMustBeEmitted(D))
      return true;
    //if (isa<RecordDecl, ClassTemplateDecl>(D))
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

void RemoveUnreferencedDecl::HandleTranslationUnit(ASTContext &Ctx)
{
  PropagateVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

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

void RemoveUnreferencedDecl::doRewriting(void)
{
  if (ToCounter <= 0) {
    if (TransformationCounter >= 1 &&
        TransformationCounter <= ValidInstanceNum) {
      auto *TheDecl = Candidates[TransformationCounter - 1];
      SourceRange Range = RewriteHelper->getDeclFullSourceRange(TheDecl);

      TheRewriter.RemoveText(Range);
    }
  } else {
    for (int I = ToCounter; I >= TransformationCounter; --I) {
      SourceRange Range = RewriteHelper->getDeclFullSourceRange(Candidates[I - 1]);

      TheRewriter.getRangeSize(Range);
      TheRewriter.getRewrittenText(Range);
      TheRewriter.RemoveText(Range);
    }
  }
}

RemoveUnreferencedDecl::~RemoveUnreferencedDecl(void)
{
  // Nothing to do
}
