//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Timo Stripf
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#ifndef REMOVE_UNREFERENCED_DECL_H
#define REMOVE_UNREFERENCED_DECL_H

#include <string>
#include "llvm/ADT/DenseMap.h"
#include "Transformation.h"

class Candidate;

class CandidateTransformation : public Transformation {
protected:
  std::vector<std::shared_ptr<Candidate>> Candidates;

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  void doRewriting();

public:
  CandidateTransformation(const char *TransName, const char *Desc,
                          bool MultipleRewritesFlag = false)
      : Transformation(TransName, Desc, MultipleRewritesFlag) {}

  RewriteUtils *getRewriterHelper() { return RewriteHelper; }
  clang::Rewriter& getRewriter() { return TheRewriter; }

  void
  CheckAndRemoveCandidates(std::vector<std::shared_ptr<Candidate>> &Candidates);

  virtual void CollectCandidates(clang::ASTContext& Ctx) = 0;
};


class Candidate {
public:
  virtual bool check(CandidateTransformation& Trans) = 0;
  virtual void apply(CandidateTransformation& Trans) = 0;
};

class RemoveUnreferencedDecl : public CandidateTransformation {
  class PropagateVisitor;
  class CollectionVisitor;

public:

  RemoveUnreferencedDecl(const char *TransName, const char *Desc,
                         bool AllAtOnce)
      : CandidateTransformation(TransName, Desc, /*MultipleRewrites*/ true),
        AllAtOnce(AllAtOnce) {}

private:
  virtual void CollectCandidates(clang::ASTContext &Ctx) override;

  bool AllAtOnce;
};
#endif
