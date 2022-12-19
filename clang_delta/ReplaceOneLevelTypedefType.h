//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013, 2016 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#ifndef REPLACE_ONE_LEVEL_TYPEDEF_TYPE_H
#define REPLACE_ONE_LEVEL_TYPEDEF_TYPE_H

#include "Transformation.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "clang/AST/TypeLoc.h"

namespace clang {
  class DeclGroupRef;
  class ASTContext;
  class TypedefNameDecl;
}

class ReplaceOneLevelTypedefType : public Transformation {
  class CollectionVisitor;

public:
  ReplaceOneLevelTypedefType(const char *TransName, const char *Desc)
      : Transformation(TransName, Desc) {}

  ~ReplaceOneLevelTypedefType(void);

private:

  virtual void Initialize(clang::ASTContext &context);

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  void handleOneTypedefTypeLoc(clang::TypedefTypeLoc TLoc);
  void handleOneTemplateSpecializationTypeLoc(clang::TypeLoc TLoc);

  void analyzeTypeLocs();

  void rewriteTypedefType(clang::TypedefTypeLoc TheTypeLoc, const clang::TypedefNameDecl* TheTypedefDecl);
  void rewriteOtherType(clang::TypeLoc TL);

  void removeTypedefs(const clang::TypedefNameDecl* TheTypedefDecl);

  std::map<const clang::TypedefNameDecl*, std::vector<clang::TypedefTypeLoc>> AllTypeDecls;

  std::vector<clang::TypedefTypeLoc> ValidTypedefTypes;
  std::vector<clang::TypeLoc> ValidOtherTypes;

  // Unimplemented
  ReplaceOneLevelTypedefType(void);

  ReplaceOneLevelTypedefType(const ReplaceOneLevelTypedefType &);

  void operator=(const ReplaceOneLevelTypedefType &);
};

#endif

