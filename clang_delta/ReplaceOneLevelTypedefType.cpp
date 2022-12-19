//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013, 2015, 2017, 2019 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ReplaceOneLevelTypedefType.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"

#include "TransformationManager.h"

using namespace clang;

static const char *DescriptionMsg =
"This pass literally replaces a typedef type with the underlying type \
if typedef type is only been used once. It doesn't recursively resolve \
the underlying type.";

static RegisterTransformation<ReplaceOneLevelTypedefType>
         Trans("replace-one-level-typedef-type", DescriptionMsg);

class ReplaceOneLevelTypedefType::CollectionVisitor
    : public RecursiveASTVisitor<CollectionVisitor> {

public:
  explicit CollectionVisitor(ReplaceOneLevelTypedefType *Instance)
      : ConsumerInstance(Instance) {}

  bool VisitTypedefTypeLoc(TypedefTypeLoc TLoc) {
    ConsumerInstance->handleOneTypedefTypeLoc(TLoc);
    return true;
  }

  bool VisitTemplateSpecializationTypeLoc(TemplateSpecializationTypeLoc TLoc) {
    ConsumerInstance->handleOneTemplateSpecializationTypeLoc(TLoc);
    return true;
  }

  bool VisitElaboratedTypeLoc(ElaboratedTypeLoc TL) {
    if (TL.getInnerType()->getAs<TypedefType>())
      ConsumerInstance->handleOneTemplateSpecializationTypeLoc(TL);
    return true;
  }

private:
  ReplaceOneLevelTypedefType *ConsumerInstance;
};

void ReplaceOneLevelTypedefType::Initialize(ASTContext &context) 
{
  Transformation::Initialize(context);
}

void ReplaceOneLevelTypedefType::HandleTranslationUnit(ASTContext &Ctx)
{
  CollectionVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());
  analyzeTypeLocs();

  ValidInstanceNum = ValidTypedefTypes.size() + ValidOtherTypes.size();

  if (QueryInstanceOnly)
    return;

  if (TransformationCounter > ValidInstanceNum) {
    TransError = TransMaxInstanceError;
    return;
  }

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  if (TransformationCounter <= (int)ValidTypedefTypes.size()) {
    auto TL = ValidTypedefTypes[TransformationCounter - 1];
    rewriteTypedefType(TL, TL.getTypedefNameDecl());
    removeTypedefs(TL.getTypedefNameDecl());
  } else {
    TransformationCounter -= ValidTypedefTypes.size();
    auto TL = ValidOtherTypes[TransformationCounter - 1];
    rewriteOtherType(TL);
  }

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

void ReplaceOneLevelTypedefType::analyzeTypeLocs()
{
  for (auto& LocVec : AllTypeDecls) {
    if (LocVec.second.size() > 1)
      continue;
    ValidTypedefTypes.push_back(LocVec.second.back());
  }
}

void ReplaceOneLevelTypedefType::rewriteTypedefType(clang::TypedefTypeLoc TheTypeLoc, const clang::TypedefNameDecl* TheTypedefDecl)
{
  std::string NewTyStr;
  TheTypedefDecl->getUnderlyingType().getAsStringInternal(NewTyStr,
                                        getPrintingPolicy());
  SourceRange Range = TheTypeLoc.getSourceRange();
  TheRewriter.ReplaceText(Range, NewTyStr);
}

void ReplaceOneLevelTypedefType::rewriteOtherType(clang::TypeLoc TL) {
  if (auto TSTL = TL.getAs<TemplateSpecializationTypeLoc>()) {
    std::string NewTyStr;
    TSTL.getTypePtr()->getAliasedType().getAsStringInternal(
        NewTyStr, getPrintingPolicy());
    SourceRange Range = TSTL.getSourceRange();
    TheRewriter.ReplaceText(Range, NewTyStr);
  } else if (auto ETL = TL.getAs<ElaboratedTypeLoc>()) {
    auto *TT = ETL.getInnerType()->getAs<TypedefType>();
    std::string NewTyStr;
    TT->getDecl()->getUnderlyingType().getAsStringInternal(NewTyStr,
                                                           getPrintingPolicy());
    SourceRange Range = ETL.getSourceRange();
    TheRewriter.ReplaceText(Range, NewTyStr);
  }
}

void ReplaceOneLevelTypedefType::removeTypedefs(const clang::TypedefNameDecl* TheTypedefDecl)
{
  for (TypedefNameDecl::redecl_iterator I = TheTypedefDecl->redecls_begin(),
       E = TheTypedefDecl->redecls_end(); I != E; ++I) {
    SourceRange Range = (*I)->getSourceRange();
    if (Range.isValid()) {
      RewriteHelper->removeTextUntil(Range, ';');
      Rewritten = true;
    }
  }
}

void ReplaceOneLevelTypedefType::handleOneTypedefTypeLoc(TypedefTypeLoc TLoc)
{
  if (isInIncludedFile(TLoc.getBeginLoc()))
    return;
  const TypedefType *TdefTy = TLoc.getTypePtr();
  const TypedefNameDecl *TdefD = TdefTy->getDecl();
  if (!TdefD || TdefD->getBeginLoc().isInvalid())
    return;
  const TypedefNameDecl *CanonicalD = 
    dyn_cast<TypedefNameDecl>(TdefD->getCanonicalDecl());

  AllTypeDecls[CanonicalD].push_back(TLoc);
}

void ReplaceOneLevelTypedefType::handleOneTemplateSpecializationTypeLoc(clang::TypeLoc TLoc)
{
  if (isInIncludedFile(TLoc.getBeginLoc()))
    return;

  ValidOtherTypes.push_back(TLoc);
}

ReplaceOneLevelTypedefType::~ReplaceOneLevelTypedefType(void)
{
}
