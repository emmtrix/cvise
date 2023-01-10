//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012 The University of Utah
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

class RemoveUnreferencedDecl : public Transformation {
  class PropagateVisitor;
  class CollectionVisitor;


public:

  RemoveUnreferencedDecl(const char *TransName, const char *Desc)
    : Transformation(TransName, Desc, /*MultipleRewrites*/true)
  { }

  ~RemoveUnreferencedDecl(void);

private:

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  void doRewriting(void);

  std::vector<clang::Decl*> Candidates;

  clang::Decl* TheDecl = nullptr;
};
#endif
