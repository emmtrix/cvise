//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Timo Stripf
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


void CandidateTransformation::CheckAndRemoveCandidates(std::vector<std::shared_ptr<Candidate>>& Candidates) {
  Candidates.erase(std::remove_if(Candidates.begin(), Candidates.end(),
                                  [this](shared_ptr<Candidate> candidate) {
                                    return !candidate->check(*this);
                                  }),
                   Candidates.end());
}

void CandidateTransformation::HandleTranslationUnit(ASTContext &Ctx) {
  CollectCandidates(Ctx);

  CheckAndRemoveCandidates(Candidates);

  ValidInstanceNum = Candidates.size();

  if (QueryInstanceOnly || !checkCounterValidity())
    return;

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  doRewriting();

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

void CandidateTransformation::doRewriting() {
  if (ToCounter <= 0) {
    if (TransformationCounter >= 1 &&
        TransformationCounter <= ValidInstanceNum) {

      Candidates[TransformationCounter - 1]->apply(*this);
    }
  } else {
    for (int I = ToCounter; I >= TransformationCounter; --I) {
      Candidates[I - 1]->apply(*this);
    }
  }
}

static const char *DescriptionMsg =
    "Remove declarations that are unreferenced with the source code. \n";

static RegisterTransformation<RemoveUnreferencedDecl, bool>
    Trans("remove-unreferenced-decl", DescriptionMsg, false);
static RegisterTransformation<RemoveUnreferencedDecl, bool>
    TransAll("remove-unreferenced-decl-all", DescriptionMsg, true);

class RemoveDeclCandidate : public Candidate {
  Decl *D;

public:
  RemoveDeclCandidate(Decl *D) : D(D) {}

  virtual bool check(CandidateTransformation &Trans) override {
    SourceRange Range = Trans.getRewriterHelper()->getDeclFullSourceRange(D);

    if (Range.isInvalid() || Trans.isInIncludedFile(Range))
      return false;

    return true;
  }

  bool isAlreadyRemoved(CandidateTransformation& Trans, SourceLocation Loc) {
    auto DL = Trans.getRewriter().getSourceMgr().getDecomposedLoc(Loc);

    SourceLocation LocBegin = Loc.getLocWithOffset(-DL.second);

    // Puuh, I found no method in clang::Rewriter to check if text is already removed
    // Checking the range size from the beginning of the file returns a negative number if one location is already deleted
    return Trans.getRewriter().getRangeSize({ LocBegin, Loc }) < 0;
  }

  virtual void apply(CandidateTransformation& Trans) override {
    SourceRange Range = Trans.getRewriterHelper()->getDeclFullSourceRange(D);
    if (isAlreadyRemoved(Trans, Range.getBegin()) ||
        isAlreadyRemoved(Trans, Range.getEnd()))
      return;

    Trans.getRewriter().RemoveText(Range);
  }
};

class GroupCandidate : public Candidate {
public:
  std::vector<std::shared_ptr<Candidate>> Candidates;

  virtual bool check(CandidateTransformation &Trans) override {
    Trans.CheckAndRemoveCandidates(Candidates);

    return !Candidates.empty();
  }

  virtual void apply(CandidateTransformation &Trans) override {
    for (auto& C : Candidates)
      C->apply(Trans);
  }
};

class RemoveUnreferencedDecl::PropagateVisitor
    : public RecursiveASTVisitor<PropagateVisitor> {
  using Base = RecursiveASTVisitor;

  RemoveUnreferencedDecl *ConsumerInstance;

  map<pair<SourceLocation, SourceLocation>, set<Decl *>> IndexedDeclGroups;
  vector<set<Decl *>> DeclGroups;
  map<const IdentifierInfo*, set<Decl*>> DTSTCandidates;
  map<Decl*, set<Decl*>> Parents;

public:
  explicit PropagateVisitor(RemoveUnreferencedDecl *Instance)
      : ConsumerInstance(Instance) {}

  bool shouldVisitTemplateInstantiations() const { return true; }
  bool shouldVisitImplicitCode() const { return true; }

  bool VisitDecl(Decl* D) {
    if (ConsumerInstance->Context->DeclMustBeEmitted(D)) {
      D->setReferenced();
      D->setIsUsed();
    }

    IndexedDeclGroups[{D->getBeginLoc(), D->getEndLoc()}].insert(D);

    if (auto* P = dyn_cast_or_null<Decl>(D->getDeclContext()))
      Parents[D].insert(P);
    if (auto *P = dyn_cast_or_null<Decl>(D->getLexicalDeclContext()))
      Parents[D].insert(P);

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

  bool VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl *TATD) {
    DTSTCandidates[TATD->getTemplatedDecl()->getIdentifier()].insert(TATD);

    return Base::VisitTypeAliasTemplateDecl(TATD);
  }

  bool VisitTemplateSpecializationType(TemplateSpecializationType *TST) {
    if (TemplateDecl *TD = TST->getTemplateName().getAsTemplateDecl()) {
      TD->setReferenced();
    }

    return Base::VisitTemplateSpecializationType(TST);
  }

  bool VisitDependentTemplateSpecializationType(
      DependentTemplateSpecializationType *DTST) {
    for (auto *D : DTSTCandidates[DTST->getIdentifier()])
      D->setReferenced();

    return Base::VisitDependentTemplateSpecializationType(DTST);
  }

  bool VisitUsingDecl(UsingDecl *UD) {
    set<Decl *> Decls{UD};
    for (UsingShadowDecl *S : UD->shadows()) {
      Decls.insert(S);
      Decls.insert(S->getTargetDecl());
    }
    DeclGroups.push_back(Decls);

    return Base::VisitUsingDecl(UD);
  }

  bool VisitUnresolvedLookupExpr(UnresolvedLookupExpr *ULE) {
    for (Decl* D : ULE->decls())
      D->setReferenced();

    return Base::VisitUnresolvedLookupExpr(ULE);
  }

  bool TraverseTemplateName(TemplateName TN) {
    //TN.getKind();

    if (auto* TD = TN.getAsTemplateDecl())
      TD->setReferenced();

    return Base::TraverseTemplateName(TN);
  }

  bool VisitRecordType(RecordType* RT) {
    RT->getDecl()->setReferenced();

    return Base::VisitRecordType(RT);
  }

  bool
  VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *CTSD) {
    if (CTSD->getSpecializationKind() == TSK_ExplicitSpecialization)
      CTSD->getSpecializedTemplate()->setReferenced();

    return Base::VisitClassTemplateSpecializationDecl(CTSD);
  }

  bool VisitOverloadExpr(OverloadExpr *OE) {
    for (auto *D : OE->decls())
      D->setReferenced();

    return Base::VisitOverloadExpr(OE);
  }

  bool setReferenced(Decl *D) {
    if (D->isReferenced())
      return false;

    D->setReferenced();
    return true;
  }

  bool setReferenced(set<Decl *> &Decls, bool Val = true) {
    bool Changed = false;

    if (Val)
      for (auto *D : Decls)
        Changed |= setReferenced(D);

    return Changed;
  }

  bool setUsed(Decl *D) {
    if (D->isUsed())
      return false;

    D->setIsUsed();
    return true;
  }

  bool setUsed(set<Decl *> &Decls, bool Val = true) {
    bool Changed = false;

    if (Val)
      for (auto *D : Decls)
        Changed |= setUsed(D);

    return Changed;
  }

  bool propagateReferenced(set<Decl *> &Decls) {
    if (Decls.size() == 1)
      return false;

    bool Referenced = false;
    for (auto *D : Decls)
      Referenced |= D->isReferenced();

    return setReferenced(Decls, Referenced);
  }

  bool propagateUsed(set<Decl *> &Decls) {
    if (Decls.size() == 1)
      return false;

    bool Used = false;
    for (auto *D : Decls)
      Used |= D->isUsed();

    return setUsed(Decls, Used);
  }

  bool propagate() {
    bool Changed = false;

    for (auto& Entry : IndexedDeclGroups) {
      Changed |= propagateReferenced(Entry.second);
      Changed |= propagateUsed(Entry.second);
    }
    for (auto& Decls : DeclGroups) {
      Changed |= propagateReferenced(Decls);
      Changed |= propagateUsed(Decls);
    }

    for (auto& Entry : Parents) {
      Changed |= setReferenced(Entry.second, Entry.first->isReferenced());
      Changed |= setUsed(Entry.second, Entry.first->isUsed());
    }

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
    if (!D->isReferenced() && isa<FunctionDecl, TypedefNameDecl, UsingDecl, RecordDecl>(D)) {
      ConsumerInstance->Candidates.push_back(
          make_shared<RemoveDeclCandidate>(D));
    }

    return true;
  }
};

void RemoveUnreferencedDecl::CollectCandidates(ASTContext &Ctx) {
  PropagateVisitor(this).start(Ctx.getTranslationUnitDecl());

  CollectionVisitor(this).TraverseDecl(Ctx.getTranslationUnitDecl());

  if (AllAtOnce) {
    auto GC = make_shared<GroupCandidate>();
    GC->Candidates = Candidates;
    Candidates = { GC };
  }
}
