//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013, 2015, 2016, 2017, 2018, 2019, 2020 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "RenameOperator.h"

#include <sstream>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"

#include "TransformationManager.h"

using namespace clang;

static const char *DescriptionMsg =
"A pass to rename operator functions (e.g. operator +) to regular function names op1, op2, ... \
Relevant operators are replaced by function calls, e.g. a + b => a.op1(b). \n";

static RegisterTransformation<RenameOperator>
         Trans("rename-operator", DescriptionMsg);

class RenameOperator::CollectionVisitor : public
  RecursiveASTVisitor<CollectionVisitor> {

public:

  explicit CollectionVisitor(RenameOperator *Instance)
    : ConsumerInstance(Instance)
  { }

  bool RenameOperator::CollectionVisitor::VisitFunctionDecl(FunctionDecl *FD)
  {
    if (!FD->isOverloadedOperator()) {
      return true;
    }

    const FunctionDecl *CanonicalFD = FD->getCanonicalDecl();
    if (ConsumerInstance->isInIncludedFile(FD) ||
      ConsumerInstance->isInIncludedFile(CanonicalFD))
      return true;

    ConsumerInstance->addFun(CanonicalFD);

    return true;
  }

private:

  RenameOperator *ConsumerInstance;

};

class RenameOperator::RenameOperatorVisitor : public RecursiveASTVisitor<RenameOperatorVisitor> {
 using Base = RecursiveASTVisitor<RenameOperatorVisitor>;
public:

  explicit RenameOperatorVisitor(RenameOperator *Instance)
    : ConsumerInstance(Instance)
  { }

  std::string* GetNewName(FunctionDecl* FD) {
    FunctionDecl *CanonicalDecl = FD->getCanonicalDecl();

    if (ConsumerInstance->RenameFunc.count(CanonicalDecl))
        return &ConsumerInstance->RenameFunc[CanonicalDecl];

    return nullptr;
  }

  bool VisitFunctionDecl(FunctionDecl* FD) {
    if (auto NewName = GetNewName(FD)) {
      ConsumerInstance->RewriteHelper->replaceFunctionDeclName(FD, *NewName);
    }

    return true;
  }

  bool TraverseCXXOperatorCallExpr(CXXOperatorCallExpr* OCE) {
    if (auto* MD = dyn_cast<CXXMethodDecl>(OCE->getCalleeDecl())) {
      if (auto NewName = GetNewName(MD)) {
        std::string OpSpelling = getOperatorSpelling(OCE->getOperator());
        if (OCE->getNumArgs() == 2) {
          ConsumerInstance->TheRewriter.ReplaceText(OCE->getOperatorLoc(), OpSpelling.size(), "." + *NewName + "(");
          ConsumerInstance->TheRewriter.InsertTextAfterToken(OCE->getArg(1)->getEndLoc(), ")");
        } else if (OCE->getNumArgs() == 1) {
          ConsumerInstance->TheRewriter.ReplaceText(OCE->getOperatorLoc(), OpSpelling.size(), "");
          ConsumerInstance->TheRewriter.InsertTextAfterToken(OCE->getArg(0)->getEndLoc(), "." + *NewName + "()");
        }
      }
    }

    // Only traverse into arguments and not into callee. That would call VisitDeclRefExpr.
    for (auto arg : OCE->arguments())
      Base::TraverseStmt(arg);

    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE)
  {
    if (ConsumerInstance->isInIncludedFile(DRE))
      return true;

    if (FunctionDecl* FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
      if (auto NewName = GetNewName(FD)) {
        ConsumerInstance->TheRewriter.ReplaceText(DRE->getNameInfo().getSourceRange(), *NewName);
      }
    }

    return true;
  }


  bool VisitMemberExpr(MemberExpr *ME)
  {
    if (ConsumerInstance->isInIncludedFile(ME))
      return true;

    if (FunctionDecl* FD = dyn_cast<FunctionDecl>(ME->getMemberDecl())) {
      if (auto NewName = GetNewName(FD)) {
        ConsumerInstance->TheRewriter.ReplaceText(ME->getMemberNameInfo().getSourceRange(), *NewName);
      }
    }

    return true;
  }

private:

  RenameOperator *ConsumerInstance;

};

void RenameOperator::Initialize(ASTContext &context) 
{
  Transformation::Initialize(context);
  ValidInstanceNum = 1;
}

std::string RenameOperator::getNextFuncName() {
  std::string Name;

  do {
    auto No = NextFunNo++;
    Name = FunNamePrefix + std::to_string(No);
  } while (UsedNames.count(Name));

  return Name;
}

void RenameOperator::HandleTranslationUnit(ASTContext &Ctx)
{
  Ctx.getTranslationUnitDecl()->dump();

  CollectionVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  ValidInstanceNum = FunctionList.size();

  if (QueryInstanceOnly) {
    return;
  }

  if (TransformationCounter > ValidInstanceNum) {
    TransError = TransMaxInstanceError;
    return;
  }

  auto Fun = FunctionList[TransformationCounter - 1];
  RenameFunc[Fun] = getNextFuncName();

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  RenameOperatorVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

void RenameOperator::addFun(const FunctionDecl *FD)
{
  FD = FD->getCanonicalDecl();
  if (!FunctionSet.count(FD)) {
    FunctionSet.insert(FD);
    FunctionList.push_back(FD);
  }
}
