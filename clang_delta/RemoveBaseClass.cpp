//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012, 2013, 2014, 2015, 2016, 2017, 2018 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "RemoveBaseClass.h"

#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include "CommonRenameClassRewriteVisitor.h"

#include "TransformationManager.h"

using namespace clang;
using namespace clang_delta_common_visitor;

static const char *DescriptionMsgRemove = 
"This pass removes a base class from a derived class. \n";

static const char* DescriptionMsgMerge =
"This pass merges a base class into a derived class if \n\
  * it has less than or equal to 5 declarations. \n\
All its declarations will be moved into one of its subclasses, \
and all references to this base class will be replaced with \
the corresponding subclass. \n";

// Note that this pass doesn't do much analysis, so
// it will produce quite a few incompilable code, especially
// when multi-inheritance is involved.

static RegisterTransformation<RemoveBaseClass>
         TransRemove("remove-base-class", DescriptionMsgRemove);
static RegisterTransformation<RemoveBaseClass>
         TransMerge("merge-base-class", DescriptionMsgMerge);

class RemoveBaseClassBaseVisitor : public 
  RecursiveASTVisitor<RemoveBaseClassBaseVisitor> {

public:
  explicit RemoveBaseClassBaseVisitor(
             RemoveBaseClass *Instance)
    : ConsumerInstance(Instance)
  { }

  bool VisitCXXRecordDecl(CXXRecordDecl *CXXRD);

private:
  RemoveBaseClass *ConsumerInstance;
};

bool RemoveBaseClassBaseVisitor::VisitCXXRecordDecl(
       CXXRecordDecl *CXXRD)
{
  ConsumerInstance->handleOneCXXRecordDecl(CXXRD);
  return true;
}

void RemoveBaseClass::Initialize(ASTContext &context) 
{
  Transformation::Initialize(context);
  CollectionVisitor = new RemoveBaseClassBaseVisitor(this);
}

void RemoveBaseClass::HandleTranslationUnit(ASTContext &Ctx)
{
  if (TransformationManager::isCLangOpt() ||
      TransformationManager::isOpenCLLangOpt()) {
    ValidInstanceNum = 0;
  }
  else {
    CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());
  }

  if (QueryInstanceOnly)
    return;

  if (TransformationCounter > ValidInstanceNum) {
    TransError = TransMaxInstanceError;
    return;
  }

  TransAssert(TheBaseClass && "TheBaseClass is NULL!");
  TransAssert(TheDerivedClass && "TheDerivedClass is NULL!");
  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  doRewrite();

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

bool RemoveBaseClass::isDirectlyDerivedFrom(const CXXRecordDecl *SubC, 
                                            const CXXRecordDecl *Base)
{
  for (CXXRecordDecl::base_class_const_iterator I = SubC->bases_begin(),
       E = SubC->bases_end(); I != E; ++I) {
    if (I->getType()->isDependentType())
      continue;

    const CXXRecordDecl *BaseDecl =
      dyn_cast<CXXRecordDecl>(I->getType()->getAs<RecordType>()->getDecl());
    if (Base->getCanonicalDecl() == BaseDecl->getCanonicalDecl())
      return true;
  }
  return false;
}

void RemoveBaseClass::handleOneCXXRecordDecl(const CXXRecordDecl *CXXRD)
{
  if (isSpecialRecordDecl(CXXRD) || !CXXRD->isThisDeclarationADefinition())
    return;

  for (const CXXBaseSpecifier& BS : CXXRD->bases()) {
    auto* Base = BS.getType()->getAsCXXRecordDecl();

    if (Base == nullptr)
      continue;
    if (Merge && getNumExplicitDecls(Base) > MaxNumDecls)
      continue;
    if (isInIncludedFile(Base))
      continue;

    ValidInstanceNum++;
    if (ValidInstanceNum == TransformationCounter) {
      TransAssert(Base->hasDefinition() && "Base class does not have any definition!");
      TheBaseClass = Base->getDefinition();
      TheDerivedClass = CXXRD;
    }
  }
}

void RemoveBaseClass::doRewrite(void)
{
  if (Merge)
    copyBaseClassDecls();
  removeBaseSpecifier();
  if (Merge)
    RewriteHelper->removeClassDecls(TheBaseClass);

  // ISSUE: I didn't handle Base initializer in a Ctor's initlist.
  //        * keeping it untouched is wrong, because delegating constructors 
  //        are only valid in c++11
  //        * naively removing the base initializer doesn't work in some cases,
  //        e.g., 
  //        class A { 
  //          A(A&) {}
  //          A &a;
  //        };
  //        class C : A {
  //          C(A &x) : A(x) {}
  //        };
  //        during transformation, removing A(x) will leave &a un-initialized.
  // I chose to simply delete the base initializer. Seemingly we will 
  // generate fewer incompilable code by doing so...
  removeBaseInitializer();
}

// ISSUE: directly copying decls could bring in name conflicts
void RemoveBaseClass::copyBaseClassDecls(void)
{
  if (!getNumExplicitDecls(TheBaseClass))
    return;

  std::string DeclsStr;
  auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(TheBaseClass);
  if (CTSD && CTSD->getSpecializationKind() == TSK_ImplicitInstantiation) {
    // For template bases, we use the printing feature of clang to generate
    // the class with all resolved template parameters

    // Rename internally the constructors to the derived class
    for (auto* D : CTSD->decls()) {
      if (auto* CD = dyn_cast<CXXConstructorDecl>(D)) {
        CD->setDeclName(TheDerivedClass->getDeclName());
      }
    }

    llvm::raw_string_ostream Strm(DeclsStr);
    CTSD->print(Strm);

    DeclsStr.erase(0, DeclsStr.find('{') + 1);
    DeclsStr.erase(DeclsStr.rfind('}'), 1);
  } else {
    SourceLocation StartLoc = TheBaseClass->getBraceRange().getBegin();
    SourceLocation EndLoc = TheBaseClass->getBraceRange().getEnd();
    TransAssert(EndLoc.isValid() && "Invalid RBraceLoc!");
    StartLoc = StartLoc.getLocWithOffset(1);
    EndLoc = EndLoc.getLocWithOffset(-1);

    DeclsStr = TheRewriter.getRewrittenText(SourceRange(StartLoc, EndLoc));
  }

  TransAssert(!DeclsStr.empty() && "Empty DeclsStr!");
  SourceLocation InsertLoc = TheDerivedClass->getBraceRange().getEnd();
  TheRewriter.InsertTextBefore(InsertLoc, DeclsStr);
}

bool RemoveBaseClass::isTheBaseClass(const CXXBaseSpecifier &Specifier)
{
  const Type *Ty = TheBaseClass->getTypeForDecl();
  return Context->hasSameType(Specifier.getType(), 
                              Ty->getCanonicalTypeInternal());
}

RemoveBaseClass::RemoveBaseClass(const char* TransName, const char* Desc) : Transformation(TransName, Desc) {
    Merge = (TransName == std::string("merge-base-class"));
}

void RemoveBaseClass::removeBaseSpecifier(void)
{
  unsigned NumBases = TheDerivedClass->getNumBases();
  TransAssert((NumBases >= 1) && "TheDerivedClass doesn't have any base!");
  if (NumBases == 1) {
    SourceLocation StartLoc = TheDerivedClass->getLocation();
    StartLoc = RewriteHelper->getLocationUntil(StartLoc, ':');
    SourceLocation EndLoc = RewriteHelper->getLocationUntil(StartLoc, '{');
    EndLoc = EndLoc.getLocWithOffset(-1);

    TheRewriter.RemoveText(SourceRange(StartLoc, EndLoc));
    return;
  }

  CXXRecordDecl::base_class_const_iterator I = TheDerivedClass->bases_begin();
  // remove 'Y,' in code like 'class X : public Y, Z {};'
  if (isTheBaseClass(*I)) {
    RewriteHelper->removeTextUntil((*I).getSourceRange(), ',');
    return;
  }

  ++I;
  CXXRecordDecl::base_class_const_iterator E = TheDerivedClass->bases_end();
  for (; I != E; ++I) {
    if (isTheBaseClass(*I)) {
      // remove ',Z' in code like 'class X : public Y, Z {};'
      SourceRange Range = (*I).getSourceRange();
      SourceLocation EndLoc = RewriteHelper->getEndLocationFromBegin(Range);
      RewriteHelper->removeTextFromLeftAt(Range, ',', EndLoc);
      return;
    }
  }
  TransAssert(0 && "Unreachable code!");
}

void RemoveBaseClass::rewriteOneCtor(const CXXConstructorDecl *Ctor)
{
  unsigned Idx = 0;
  const CXXCtorInitializer *Init = NULL;
  for (CXXConstructorDecl::init_const_iterator I = Ctor->init_begin(),
       E = Ctor->init_end(); I != E; ++I) {
    if (!(*I)->isWritten())
      continue;

    if ((*I)->isBaseInitializer()) {
      const Type *Ty = (*I)->getBaseClass();
      TransAssert(Ty && "Invalid Base Class Type!");
      if (Context->hasSameType(Ty->getCanonicalTypeInternal(),
            TheBaseClass->getTypeForDecl()->getCanonicalTypeInternal())) {
        Init = (*I);
        break;
      }
    }
    Idx++;
  }
  if (Init) {
    RewriteHelper->removeCXXCtorInitializer(Init, Idx,
                     getNumCtorWrittenInitializers(*Ctor));
  }
}

void RemoveBaseClass::removeBaseInitializer(void)
{
  for (Decl* D : TheDerivedClass->decls()) {
    if (auto* FTD = dyn_cast<FunctionTemplateDecl>(D))
      D =FTD->getTemplatedDecl();
    if (auto* Ctor = dyn_cast<CXXConstructorDecl>(D))
      if (Ctor->isThisDeclarationADefinition() && !Ctor->isDefaulted())
        rewriteOneCtor(Ctor);
  }
}

RemoveBaseClass::~RemoveBaseClass(void)
{
  delete CollectionVisitor;
}

