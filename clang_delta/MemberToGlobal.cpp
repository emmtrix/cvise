//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013, 2014, 2015 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "MemberToGlobal.h"

#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"

#include "TransformationManager.h"

using namespace clang;

static const char *DescriptionMsg =
"TODO\n";

static RegisterTransformation<MemberToGlobal>
         Trans("member-to-global", DescriptionMsg);

class MemberToGlobal::CollectionVisitor : public 
  RecursiveASTVisitor<CollectionVisitor> {

public:
  explicit CollectionVisitor(MemberToGlobal *Instance)
    : ConsumerInstance(Instance)
  { }

  bool VisitRecordDecl(RecordDecl* RD) {
    for (auto* D : RD->decls())
      ConsumerInstance->ValidDecls.push_back(std::make_pair(RD, D));

    return true;
  }

private:
  MemberToGlobal *ConsumerInstance;
};

class MemberToGlobal::RewriteVisitor : public
	RecursiveASTVisitor<RewriteVisitor> {

public:
  explicit RewriteVisitor(MemberToGlobal *Instance)
    : ConsumerInstance(Instance)
  { }

  bool VisitDeclRefExpr(DeclRefExpr *ParmRefExpr);

private:
  MemberToGlobal *ConsumerInstance;
};

void MemberToGlobal::Initialize(ASTContext &context) 
{
  Transformation::Initialize(context);
}

StringRef MemberToGlobal::GetText(SourceRange replacementRange) {
  std::pair<FileID, unsigned> Begin = SrcManager->getDecomposedLoc(replacementRange.getBegin());
  std::pair<FileID, unsigned> End = SrcManager->getDecomposedLoc(replacementRange.getEnd());
  if (Begin.first != End.first)
    return "";

  StringRef MB = SrcManager->getBufferData(Begin.first);
  return MB.substr(Begin.second, End.second - Begin.second + 1);
}

void MemberToGlobal::HandleTranslationUnit(ASTContext &Ctx)
{
  CollectionVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  ValidInstanceNum = ValidDecls.size();

  if (QueryInstanceOnly)
    return;

  if (TransformationCounter > ValidInstanceNum) {
    TransError = TransMaxInstanceError;
    return;
  }

  TheDecl = ValidDecls[TransformationCounter - 1].second;
  TheRecordDecl = ValidDecls[TransformationCounter - 1].first;
  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  auto RecordBegin = TheRecordDecl->getSourceRange().getBegin();
  auto BeginLoc = TheDecl->getSourceRange().getBegin();
  auto EndLoc = RewriteHelper->getEndLocationUntil(TheDecl->getSourceRange().getEnd(), ';');

  std::string Text = GetText(SourceRange(BeginLoc, EndLoc)).str();
  TheRewriter.InsertTextBefore(RecordBegin, Text + "\n");
  TheRewriter.RemoveText(SourceRange(BeginLoc, EndLoc));

  //RewriteVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}
