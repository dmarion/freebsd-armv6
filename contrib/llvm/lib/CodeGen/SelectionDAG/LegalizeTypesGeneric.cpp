//===-------- LegalizeTypesGeneric.cpp - Generic type legalization --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements generic type expansion and splitting for LegalizeTypes.
// The routines here perform legalization when the details of the type (such as
// whether it is an integer or a float) do not matter.
// Expansion is the act of changing a computation in an illegal type to be a
// computation in two identical registers of a smaller type.  The Lo/Hi part
// is required to be stored first in memory on little/big-endian machines.
// Splitting is the act of changing a computation in an illegal type to be a
// computation in two not necessarily identical registers of a smaller type.
// There are no requirements on how the type is represented in memory.
//
//===----------------------------------------------------------------------===//

#include "LegalizeTypes.h"
#include "llvm/Target/TargetData.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Generic Result Expansion.
//===----------------------------------------------------------------------===//

// These routines assume that the Lo/Hi part is stored first in memory on
// little/big-endian machines, followed by the Hi/Lo part.  This means that
// they cannot be used as is on vectors, for which Lo is always stored first.
void DAGTypeLegalizer::ExpandRes_MERGE_VALUES(SDNode *N, unsigned ResNo,
                                              SDValue &Lo, SDValue &Hi) {
  SDValue Op = DisintegrateMERGE_VALUES(N, ResNo);
  GetExpandedOp(Op, Lo, Hi);
}

void DAGTypeLegalizer::ExpandRes_BITCAST(SDNode *N, SDValue &Lo, SDValue &Hi) {
  EVT OutVT = N->getValueType(0);
  EVT NOutVT = TLI.getTypeToTransformTo(*DAG.getContext(), OutVT);
  SDValue InOp = N->getOperand(0);
  EVT InVT = InOp.getValueType();
  DebugLoc dl = N->getDebugLoc();

  // Handle some special cases efficiently.
  switch (getTypeAction(InVT)) {
    case TargetLowering::TypeLegal:
    case TargetLowering::TypePromoteInteger:
      break;
    case TargetLowering::TypeSoftenFloat:
      // Convert the integer operand instead.
      SplitInteger(GetSoftenedFloat(InOp), Lo, Hi);
      Lo = DAG.getNode(ISD::BITCAST, dl, NOutVT, Lo);
      Hi = DAG.getNode(ISD::BITCAST, dl, NOutVT, Hi);
      return;
    case TargetLowering::TypeExpandInteger:
    case TargetLowering::TypeExpandFloat:
      // Convert the expanded pieces of the input.
      GetExpandedOp(InOp, Lo, Hi);
      Lo = DAG.getNode(ISD::BITCAST, dl, NOutVT, Lo);
      Hi = DAG.getNode(ISD::BITCAST, dl, NOutVT, Hi);
      return;
    case TargetLowering::TypeSplitVector:
      GetSplitVector(InOp, Lo, Hi);
      if (TLI.isBigEndian())
        std::swap(Lo, Hi);
      Lo = DAG.getNode(ISD::BITCAST, dl, NOutVT, Lo);
      Hi = DAG.getNode(ISD::BITCAST, dl, NOutVT, Hi);
      return;
    case TargetLowering::TypeScalarizeVector:
      // Convert the element instead.
      SplitInteger(BitConvertToInteger(GetScalarizedVector(InOp)), Lo, Hi);
      Lo = DAG.getNode(ISD::BITCAST, dl, NOutVT, Lo);
      Hi = DAG.getNode(ISD::BITCAST, dl, NOutVT, Hi);
      return;
    case TargetLowering::TypeWidenVector: {
      assert(!(InVT.getVectorNumElements() & 1) && "Unsupported BITCAST");
      InOp = GetWidenedVector(InOp);
      EVT InNVT = EVT::getVectorVT(*DAG.getContext(), InVT.getVectorElementType(),
                                   InVT.getVectorNumElements()/2);
      Lo = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dl, InNVT, InOp,
                       DAG.getIntPtrConstant(0));
      Hi = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dl, InNVT, InOp,
                       DAG.getIntPtrConstant(InNVT.getVectorNumElements()));
      if (TLI.isBigEndian())
        std::swap(Lo, Hi);
      Lo = DAG.getNode(ISD::BITCAST, dl, NOutVT, Lo);
      Hi = DAG.getNode(ISD::BITCAST, dl, NOutVT, Hi);
      return;
    }
  }

  if (InVT.isVector() && OutVT.isInteger()) {
    // Handle cases like i64 = BITCAST v1i64 on x86, where the operand
    // is legal but the result is not.
    EVT NVT = EVT::getVectorVT(*DAG.getContext(), NOutVT, 2);

    if (isTypeLegal(NVT)) {
      SDValue CastInOp = DAG.getNode(ISD::BITCAST, dl, NVT, InOp);
      Lo = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dl, NOutVT, CastInOp,
                       DAG.getIntPtrConstant(0));
      Hi = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dl, NOutVT, CastInOp,
                       DAG.getIntPtrConstant(1));

      if (TLI.isBigEndian())
        std::swap(Lo, Hi);

      return;
    }
  }

  // Lower the bit-convert to a store/load from the stack.
  assert(NOutVT.isByteSized() && "Expanded type not byte sized!");

  // Create the stack frame object.  Make sure it is aligned for both
  // the source and expanded destination types.
  unsigned Alignment =
    TLI.getTargetData()->getPrefTypeAlignment(NOutVT.
                                              getTypeForEVT(*DAG.getContext()));
  SDValue StackPtr = DAG.CreateStackTemporary(InVT, Alignment);
  int SPFI = cast<FrameIndexSDNode>(StackPtr.getNode())->getIndex();
  MachinePointerInfo PtrInfo = MachinePointerInfo::getFixedStack(SPFI);

  // Emit a store to the stack slot.
  SDValue Store = DAG.getStore(DAG.getEntryNode(), dl, InOp, StackPtr, PtrInfo,
                               false, false, 0);

  // Load the first half from the stack slot.
  Lo = DAG.getLoad(NOutVT, dl, Store, StackPtr, PtrInfo, 
                   false, false, false, 0);

  // Increment the pointer to the other half.
  unsigned IncrementSize = NOutVT.getSizeInBits() / 8;
  StackPtr = DAG.getNode(ISD::ADD, dl, StackPtr.getValueType(), StackPtr,
                         DAG.getIntPtrConstant(IncrementSize));

  // Load the second half from the stack slot.
  Hi = DAG.getLoad(NOutVT, dl, Store, StackPtr,
                   PtrInfo.getWithOffset(IncrementSize), false,
                   false, false, MinAlign(Alignment, IncrementSize));

  // Handle endianness of the load.
  if (TLI.isBigEndian())
    std::swap(Lo, Hi);
}

void DAGTypeLegalizer::ExpandRes_BUILD_PAIR(SDNode *N, SDValue &Lo,
                                            SDValue &Hi) {
  // Return the operands.
  Lo = N->getOperand(0);
  Hi = N->getOperand(1);
}

void DAGTypeLegalizer::ExpandRes_EXTRACT_ELEMENT(SDNode *N, SDValue &Lo,
                                                 SDValue &Hi) {
  GetExpandedOp(N->getOperand(0), Lo, Hi);
  SDValue Part = cast<ConstantSDNode>(N->getOperand(1))->getZExtValue() ?
                   Hi : Lo;

  assert(Part.getValueType() == N->getValueType(0) &&
         "Type twice as big as expanded type not itself expanded!");

  GetPairElements(Part, Lo, Hi);
}

void DAGTypeLegalizer::ExpandRes_EXTRACT_VECTOR_ELT(SDNode *N, SDValue &Lo,
                                                    SDValue &Hi) {
  SDValue OldVec = N->getOperand(0);
  unsigned OldElts = OldVec.getValueType().getVectorNumElements();
  DebugLoc dl = N->getDebugLoc();

  // Convert to a vector of the expanded element type, for example
  // <3 x i64> -> <6 x i32>.
  EVT OldVT = N->getValueType(0);
  EVT NewVT = TLI.getTypeToTransformTo(*DAG.getContext(), OldVT);

  SDValue NewVec = DAG.getNode(ISD::BITCAST, dl,
                               EVT::getVectorVT(*DAG.getContext(),
                                                NewVT, 2*OldElts),
                               OldVec);

  // Extract the elements at 2 * Idx and 2 * Idx + 1 from the new vector.
  SDValue Idx = N->getOperand(1);

  // Make sure the type of Idx is big enough to hold the new values.
  if (Idx.getValueType().bitsLT(TLI.getPointerTy()))
    Idx = DAG.getNode(ISD::ZERO_EXTEND, dl, TLI.getPointerTy(), Idx);

  Idx = DAG.getNode(ISD::ADD, dl, Idx.getValueType(), Idx, Idx);
  Lo = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dl, NewVT, NewVec, Idx);

  Idx = DAG.getNode(ISD::ADD, dl, Idx.getValueType(), Idx,
                    DAG.getConstant(1, Idx.getValueType()));
  Hi = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, dl, NewVT, NewVec, Idx);

  if (TLI.isBigEndian())
    std::swap(Lo, Hi);
}

void DAGTypeLegalizer::ExpandRes_NormalLoad(SDNode *N, SDValue &Lo,
                                            SDValue &Hi) {
  assert(ISD::isNormalLoad(N) && "This routine only for normal loads!");
  DebugLoc dl = N->getDebugLoc();

  LoadSDNode *LD = cast<LoadSDNode>(N);
  EVT NVT = TLI.getTypeToTransformTo(*DAG.getContext(), LD->getValueType(0));
  SDValue Chain = LD->getChain();
  SDValue Ptr = LD->getBasePtr();
  unsigned Alignment = LD->getAlignment();
  bool isVolatile = LD->isVolatile();
  bool isNonTemporal = LD->isNonTemporal();
  bool isInvariant = LD->isInvariant();

  assert(NVT.isByteSized() && "Expanded type not byte sized!");

  Lo = DAG.getLoad(NVT, dl, Chain, Ptr, LD->getPointerInfo(),
                   isVolatile, isNonTemporal, isInvariant, Alignment);

  // Increment the pointer to the other half.
  unsigned IncrementSize = NVT.getSizeInBits() / 8;
  Ptr = DAG.getNode(ISD::ADD, dl, Ptr.getValueType(), Ptr,
                    DAG.getIntPtrConstant(IncrementSize));
  Hi = DAG.getLoad(NVT, dl, Chain, Ptr,
                   LD->getPointerInfo().getWithOffset(IncrementSize),
                   isVolatile, isNonTemporal, isInvariant,
                   MinAlign(Alignment, IncrementSize));

  // Build a factor node to remember that this load is independent of the
  // other one.
  Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Lo.getValue(1),
                      Hi.getValue(1));

  // Handle endianness of the load.
  if (TLI.isBigEndian())
    std::swap(Lo, Hi);

  // Modified the chain - switch anything that used the old chain to use
  // the new one.
  ReplaceValueWith(SDValue(N, 1), Chain);
}

void DAGTypeLegalizer::ExpandRes_VAARG(SDNode *N, SDValue &Lo, SDValue &Hi) {
  EVT OVT = N->getValueType(0);
  EVT NVT = TLI.getTypeToTransformTo(*DAG.getContext(), OVT);
  SDValue Chain = N->getOperand(0);
  SDValue Ptr = N->getOperand(1);
  DebugLoc dl = N->getDebugLoc();
  const unsigned Align = N->getConstantOperandVal(3);

  Lo = DAG.getVAArg(NVT, dl, Chain, Ptr, N->getOperand(2), Align);
  Hi = DAG.getVAArg(NVT, dl, Lo.getValue(1), Ptr, N->getOperand(2), 0);

  // Handle endianness of the load.
  if (TLI.isBigEndian())
    std::swap(Lo, Hi);

  // Modified the chain - switch anything that used the old chain to use
  // the new one.
  ReplaceValueWith(SDValue(N, 1), Hi.getValue(1));
}


//===--------------------------------------------------------------------===//
// Generic Operand Expansion.
//===--------------------------------------------------------------------===//

SDValue DAGTypeLegalizer::ExpandOp_BITCAST(SDNode *N) {
  DebugLoc dl = N->getDebugLoc();
  if (N->getValueType(0).isVector()) {
    // An illegal expanding type is being converted to a legal vector type.
    // Make a two element vector out of the expanded parts and convert that
    // instead, but only if the new vector type is legal (otherwise there
    // is no point, and it might create expansion loops).  For example, on
    // x86 this turns v1i64 = BITCAST i64 into v1i64 = BITCAST v2i32.
    EVT OVT = N->getOperand(0).getValueType();
    EVT NVT = EVT::getVectorVT(*DAG.getContext(),
                               TLI.getTypeToTransformTo(*DAG.getContext(), OVT),
                               2);

    if (isTypeLegal(NVT)) {
      SDValue Parts[2];
      GetExpandedOp(N->getOperand(0), Parts[0], Parts[1]);

      if (TLI.isBigEndian())
        std::swap(Parts[0], Parts[1]);

      SDValue Vec = DAG.getNode(ISD::BUILD_VECTOR, dl, NVT, Parts, 2);
      return DAG.getNode(ISD::BITCAST, dl, N->getValueType(0), Vec);
    }
  }

  // Otherwise, store to a temporary and load out again as the new type.
  return CreateStackStoreLoad(N->getOperand(0), N->getValueType(0));
}

SDValue DAGTypeLegalizer::ExpandOp_BUILD_VECTOR(SDNode *N) {
  // The vector type is legal but the element type needs expansion.
  EVT VecVT = N->getValueType(0);
  unsigned NumElts = VecVT.getVectorNumElements();
  EVT OldVT = N->getOperand(0).getValueType();
  EVT NewVT = TLI.getTypeToTransformTo(*DAG.getContext(), OldVT);
  DebugLoc dl = N->getDebugLoc();

  assert(OldVT == VecVT.getVectorElementType() &&
         "BUILD_VECTOR operand type doesn't match vector element type!");

  // Build a vector of twice the length out of the expanded elements.
  // For example <3 x i64> -> <6 x i32>.
  std::vector<SDValue> NewElts;
  NewElts.reserve(NumElts*2);

  for (unsigned i = 0; i < NumElts; ++i) {
    SDValue Lo, Hi;
    GetExpandedOp(N->getOperand(i), Lo, Hi);
    if (TLI.isBigEndian())
      std::swap(Lo, Hi);
    NewElts.push_back(Lo);
    NewElts.push_back(Hi);
  }

  SDValue NewVec = DAG.getNode(ISD::BUILD_VECTOR, dl,
                               EVT::getVectorVT(*DAG.getContext(),
                                                NewVT, NewElts.size()),
                               &NewElts[0], NewElts.size());

  // Convert the new vector to the old vector type.
  return DAG.getNode(ISD::BITCAST, dl, VecVT, NewVec);
}

SDValue DAGTypeLegalizer::ExpandOp_EXTRACT_ELEMENT(SDNode *N) {
  SDValue Lo, Hi;
  GetExpandedOp(N->getOperand(0), Lo, Hi);
  return cast<ConstantSDNode>(N->getOperand(1))->getZExtValue() ? Hi : Lo;
}

SDValue DAGTypeLegalizer::ExpandOp_INSERT_VECTOR_ELT(SDNode *N) {
  // The vector type is legal but the element type needs expansion.
  EVT VecVT = N->getValueType(0);
  unsigned NumElts = VecVT.getVectorNumElements();
  DebugLoc dl = N->getDebugLoc();

  SDValue Val = N->getOperand(1);
  EVT OldEVT = Val.getValueType();
  EVT NewEVT = TLI.getTypeToTransformTo(*DAG.getContext(), OldEVT);

  assert(OldEVT == VecVT.getVectorElementType() &&
         "Inserted element type doesn't match vector element type!");

  // Bitconvert to a vector of twice the length with elements of the expanded
  // type, insert the expanded vector elements, and then convert back.
  EVT NewVecVT = EVT::getVectorVT(*DAG.getContext(), NewEVT, NumElts*2);
  SDValue NewVec = DAG.getNode(ISD::BITCAST, dl,
                               NewVecVT, N->getOperand(0));

  SDValue Lo, Hi;
  GetExpandedOp(Val, Lo, Hi);
  if (TLI.isBigEndian())
    std::swap(Lo, Hi);

  SDValue Idx = N->getOperand(2);
  Idx = DAG.getNode(ISD::ADD, dl, Idx.getValueType(), Idx, Idx);
  NewVec = DAG.getNode(ISD::INSERT_VECTOR_ELT, dl, NewVecVT, NewVec, Lo, Idx);
  Idx = DAG.getNode(ISD::ADD, dl,
                    Idx.getValueType(), Idx, DAG.getIntPtrConstant(1));
  NewVec =  DAG.getNode(ISD::INSERT_VECTOR_ELT, dl, NewVecVT, NewVec, Hi, Idx);

  // Convert the new vector to the old vector type.
  return DAG.getNode(ISD::BITCAST, dl, VecVT, NewVec);
}

SDValue DAGTypeLegalizer::ExpandOp_SCALAR_TO_VECTOR(SDNode *N) {
  DebugLoc dl = N->getDebugLoc();
  EVT VT = N->getValueType(0);
  assert(VT.getVectorElementType() == N->getOperand(0).getValueType() &&
         "SCALAR_TO_VECTOR operand type doesn't match vector element type!");
  unsigned NumElts = VT.getVectorNumElements();
  SmallVector<SDValue, 16> Ops(NumElts);
  Ops[0] = N->getOperand(0);
  SDValue UndefVal = DAG.getUNDEF(Ops[0].getValueType());
  for (unsigned i = 1; i < NumElts; ++i)
    Ops[i] = UndefVal;
  return DAG.getNode(ISD::BUILD_VECTOR, dl, VT, &Ops[0], NumElts);
}

SDValue DAGTypeLegalizer::ExpandOp_NormalStore(SDNode *N, unsigned OpNo) {
  assert(ISD::isNormalStore(N) && "This routine only for normal stores!");
  assert(OpNo == 1 && "Can only expand the stored value so far");
  DebugLoc dl = N->getDebugLoc();

  StoreSDNode *St = cast<StoreSDNode>(N);
  EVT NVT = TLI.getTypeToTransformTo(*DAG.getContext(),
                                     St->getValue().getValueType());
  SDValue Chain = St->getChain();
  SDValue Ptr = St->getBasePtr();
  unsigned Alignment = St->getAlignment();
  bool isVolatile = St->isVolatile();
  bool isNonTemporal = St->isNonTemporal();

  assert(NVT.isByteSized() && "Expanded type not byte sized!");
  unsigned IncrementSize = NVT.getSizeInBits() / 8;

  SDValue Lo, Hi;
  GetExpandedOp(St->getValue(), Lo, Hi);

  if (TLI.isBigEndian())
    std::swap(Lo, Hi);

  Lo = DAG.getStore(Chain, dl, Lo, Ptr, St->getPointerInfo(),
                    isVolatile, isNonTemporal, Alignment);

  Ptr = DAG.getNode(ISD::ADD, dl, Ptr.getValueType(), Ptr,
                    DAG.getIntPtrConstant(IncrementSize));
  assert(isTypeLegal(Ptr.getValueType()) && "Pointers must be legal!");
  Hi = DAG.getStore(Chain, dl, Hi, Ptr,
                    St->getPointerInfo().getWithOffset(IncrementSize),
                    isVolatile, isNonTemporal,
                    MinAlign(Alignment, IncrementSize));

  return DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Lo, Hi);
}


//===--------------------------------------------------------------------===//
// Generic Result Splitting.
//===--------------------------------------------------------------------===//

// Be careful to make no assumptions about which of Lo/Hi is stored first in
// memory (for vectors it is always Lo first followed by Hi in the following
// bytes; for integers and floats it is Lo first if and only if the machine is
// little-endian).

void DAGTypeLegalizer::SplitRes_MERGE_VALUES(SDNode *N, unsigned ResNo,
                                             SDValue &Lo, SDValue &Hi) {
  SDValue Op = DisintegrateMERGE_VALUES(N, ResNo);
  GetSplitOp(Op, Lo, Hi);
}

void DAGTypeLegalizer::SplitRes_SELECT(SDNode *N, SDValue &Lo,
                                       SDValue &Hi) {
  SDValue LL, LH, RL, RH, CL, CH;
  DebugLoc dl = N->getDebugLoc();
  GetSplitOp(N->getOperand(1), LL, LH);
  GetSplitOp(N->getOperand(2), RL, RH);

  SDValue Cond = N->getOperand(0);
  CL = CH = Cond;
  if (Cond.getValueType().isVector()) {
    assert(Cond.getValueType().getVectorElementType() == MVT::i1 &&
           "Condition legalized before result?");
    unsigned NumElements = Cond.getValueType().getVectorNumElements();
    EVT VCondTy = EVT::getVectorVT(*DAG.getContext(), MVT::i1, NumElements / 2);
    CL = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dl, VCondTy, Cond,
                     DAG.getIntPtrConstant(0));
    CH = DAG.getNode(ISD::EXTRACT_SUBVECTOR, dl, VCondTy, Cond,
                     DAG.getIntPtrConstant(NumElements / 2));
  }

  Lo = DAG.getNode(N->getOpcode(), dl, LL.getValueType(), CL, LL, RL);
  Hi = DAG.getNode(N->getOpcode(), dl, LH.getValueType(), CH, LH, RH);
}

void DAGTypeLegalizer::SplitRes_SELECT_CC(SDNode *N, SDValue &Lo,
                                          SDValue &Hi) {
  SDValue LL, LH, RL, RH;
  DebugLoc dl = N->getDebugLoc();
  GetSplitOp(N->getOperand(2), LL, LH);
  GetSplitOp(N->getOperand(3), RL, RH);

  Lo = DAG.getNode(ISD::SELECT_CC, dl, LL.getValueType(), N->getOperand(0),
                   N->getOperand(1), LL, RL, N->getOperand(4));
  Hi = DAG.getNode(ISD::SELECT_CC, dl, LH.getValueType(), N->getOperand(0),
                   N->getOperand(1), LH, RH, N->getOperand(4));
}

void DAGTypeLegalizer::SplitRes_UNDEF(SDNode *N, SDValue &Lo, SDValue &Hi) {
  EVT LoVT, HiVT;
  GetSplitDestVTs(N->getValueType(0), LoVT, HiVT);
  Lo = DAG.getUNDEF(LoVT);
  Hi = DAG.getUNDEF(HiVT);
}
