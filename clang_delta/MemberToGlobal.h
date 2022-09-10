//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#ifndef MEMBER_TO_GLOBAL_H
#define MEMBER_TO_GLOBAL_H

#include <string>
#include "llvm/ADT/SmallVector.h"
#include "Transformation.h"
#include "CommonParameterRewriteVisitor.h"

namespace clang {
  class DeclGroupRef;
  class ASTContext;
  class FunctionDecl;
  class ReturnStmt;
  class ParmVarDecl;
}

class ParamToGlobalASTVisitor;
class ParamToGlobalRewriteVisitor;
template<typename T, typename Trans> class CommonParameterRewriteVisitor;

class MemberToGlobal : public Transformation {

  class CollectionVisitor;
  class RewriteVisitor;
public:

  MemberToGlobal(const char *TransName, const char *Desc)
    : Transformation(TransName, Desc)
  { }

private:
  
  virtual void Initialize(clang::ASTContext &context);

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  llvm::StringRef GetText(clang::SourceRange range);

  std::vector<std::pair<clang::RecordDecl*, clang::Decl*>> ValidDecls;

  clang::Decl *TheDecl = nullptr;
  clang::RecordDecl *TheRecordDecl = nullptr;
};
#endif
