// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDED_FROM_MACRO_ASSEMBLER_H
#error This header must be included via macro-assembler.h
#endif

#ifndef V8_CODEGEN_IA32_MACRO_ASSEMBLER_IA32_H_
#define V8_CODEGEN_IA32_MACRO_ASSEMBLER_IA32_H_

#include <stdint.h>

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/codegen/assembler.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/ia32/assembler-ia32.h"
#include "src/codegen/ia32/register-ia32.h"
#include "src/codegen/label.h"
#include "src/codegen/reglist.h"
#include "src/codegen/reloc-info.h"
#include "src/codegen/turbo-assembler.h"
#include "src/common/globals.h"
#include "src/execution/frames.h"
#include "src/handles/handles.h"
#include "src/objects/heap-object.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class Code;
class ExternalReference;
class StatsCounter;

// Convenience for platform-independent signatures.  We do not normally
// distinguish memory operands from other operands on ia32.
using MemOperand = Operand;

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };

// TODO(victorgomes): Move definition to macro-assembler.h, once all other
// platforms are updated.
enum class StackLimitKind { kInterruptStackLimit, kRealStackLimit };

// Convenient class to access arguments below the stack pointer.
class StackArgumentsAccessor {
 public:
  // argc = the number of arguments not including the receiver.
  explicit StackArgumentsAccessor(Register argc) : argc_(argc) {
    DCHECK_NE(argc_, no_reg);
  }

  // Argument 0 is the receiver (despite argc not including the receiver).
  Operand operator[](int index) const { return GetArgumentOperand(index); }

  Operand GetArgumentOperand(int index) const;
  Operand GetReceiverOperand() const { return GetArgumentOperand(0); }

 private:
  const Register argc_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackArgumentsAccessor);
};

class V8_EXPORT_PRIVATE TurboAssembler : public TurboAssemblerBase {
 public:
  using TurboAssemblerBase::TurboAssemblerBase;

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg) {
    // Out-of-line constant pool not implemented on ia32.
    UNREACHABLE();
  }
  void LeaveFrame(StackFrame::Type type);

// Allocate stack space of given size (i.e. decrement {esp} by the value
// stored in the given register, or by a constant). If you need to perform a
// stack check, do it before calling this function because this function may
// write into the newly allocated space. It may also overwrite the given
// register's value, in the version that takes a register.
#ifdef V8_OS_WIN
  void AllocateStackSpace(Register bytes_scratch);
  void AllocateStackSpace(int bytes);
#else
  void AllocateStackSpace(Register bytes) { sub(esp, bytes); }
  void AllocateStackSpace(int bytes) {
    DCHECK_GE(bytes, 0);
    if (bytes == 0) return;
    sub(esp, Immediate(bytes));
  }
#endif

  // Print a message to stdout and abort execution.
  void Abort(AbortReason reason);

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, AbortReason reason);

  // Like Assert(), but without condition.
  // Use --debug_code to enable.
  void AssertUnreachable(AbortReason reason);

  // Like Assert(), but always enabled.
  void Check(Condition cc, AbortReason reason);

  // Check that the stack is aligned.
  void CheckStackAlignment();

  // Move a constant into a destination using the most efficient encoding.
  void Move(Register dst, const Immediate& src);
  void Move(Register dst, Smi src) { Move(dst, Immediate(src)); }
  void Move(Register dst, Handle<HeapObject> src);
  void Move(Register dst, Register src);
  void Move(Register dst, Operand src);
  void Move(Operand dst, const Immediate& src);

  // Move an immediate into an XMM register.
  void Move(XMMRegister dst, uint32_t src);
  void Move(XMMRegister dst, uint64_t src);
  void Move(XMMRegister dst, float src) { Move(dst, bit_cast<uint32_t>(src)); }
  void Move(XMMRegister dst, double src) { Move(dst, bit_cast<uint64_t>(src)); }

  Operand EntryFromBuiltinIndexAsOperand(Builtins::Name builtin_index);

  void Call(Register reg) { call(reg); }
  void Call(Operand op) { call(op); }
  void Call(Label* target) { call(target); }
  void Call(Handle<Code> code_object, RelocInfo::Mode rmode);

  // Load the builtin given by the Smi in |builtin_index| into the same
  // register.
  void LoadEntryFromBuiltinIndex(Register builtin_index);
  void CallBuiltinByIndex(Register builtin_index) override;
  void CallBuiltin(int builtin_index);

  void LoadCodeObjectEntry(Register destination, Register code_object) override;
  void CallCodeObject(Register code_object) override;
  void JumpCodeObject(Register code_object,
                      JumpMode jump_mode = JumpMode::kJump) override;
  void Jump(const ExternalReference& reference) override;

  void RetpolineCall(Register reg);
  void RetpolineCall(Address destination, RelocInfo::Mode rmode);

  void Jump(Handle<Code> code_object, RelocInfo::Mode rmode);

  void LoadMap(Register destination, Register object);

  void RetpolineJump(Register reg);

  void Trap() override;
  void DebugBreak() override;

  void CallForDeoptimization(Builtins::Name target, int deopt_id, Label* exit,
                             DeoptimizeKind kind, Label* ret,
                             Label* jump_deoptimization_entry_label);

  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }
  // Jump if the operand is a smi.
  inline void JumpIfSmi(Operand value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }

  void JumpIfEqual(Register a, int32_t b, Label* dest) {
    cmp(a, Immediate(b));
    j(equal, dest);
  }

  void JumpIfLessThan(Register a, int32_t b, Label* dest) {
    cmp(a, Immediate(b));
    j(less, dest);
  }

  void SmiUntag(Register reg) { sar(reg, kSmiTagSize); }
  void SmiUntag(Register output, Register value) {
    mov(output, value);
    SmiUntag(output);
  }

  // Removes current frame and its arguments from the stack preserving the
  // arguments and a return address pushed to the stack for the next call. Both
  // |callee_args_count| and |caller_args_count| do not include receiver.
  // |callee_args_count| is not modified. |caller_args_count| is trashed.
  // |number_of_temp_values_after_return_address| specifies the number of words
  // pushed to the stack after the return address. This is to allow "allocation"
  // of scratch registers that this function requires by saving their values on
  // the stack.
  void PrepareForTailCall(Register callee_args_count,
                          Register caller_args_count, Register scratch0,
                          Register scratch1,
                          int number_of_temp_values_after_return_address);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in esp[0], esp[4],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_arguments, Register scratch);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);

  void ShlPair(Register high, Register low, uint8_t imm8);
  void ShlPair_cl(Register high, Register low);
  void ShrPair(Register high, Register low, uint8_t imm8);
  void ShrPair_cl(Register high, Register low);
  void SarPair(Register high, Register low, uint8_t imm8);
  void SarPair_cl(Register high, Register low);

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue();

  void Lzcnt(Register dst, Register src) { Lzcnt(dst, Operand(src)); }
  void Lzcnt(Register dst, Operand src);

  void Tzcnt(Register dst, Register src) { Tzcnt(dst, Operand(src)); }
  void Tzcnt(Register dst, Operand src);

  void Popcnt(Register dst, Register src) { Popcnt(dst, Operand(src)); }
  void Popcnt(Register dst, Operand src);

  void PushReturnAddressFrom(Register src) { push(src); }
  void PopReturnAddressTo(Register dst) { pop(dst); }

  void PushReturnAddressFrom(XMMRegister src, Register scratch) {
    Push(src, scratch);
  }
  void PopReturnAddressTo(XMMRegister dst, Register scratch) {
    Pop(dst, scratch);
  }

  void Ret();

  // Root register utility functions.

  void InitializeRootRegister();

  void LoadRoot(Register destination, RootIndex index) override;

  // Indirect root-relative loads.
  void LoadFromConstantsTable(Register destination,
                              int constant_index) override;
  void LoadRootRegisterOffset(Register destination, intptr_t offset) override;
  void LoadRootRelative(Register destination, int32_t offset) override;

  void PushPC();

  enum class PushArrayOrder { kNormal, kReverse };
  // `array` points to the first element (the lowest address).
  // `array` and `size` are not modified.
  void PushArray(Register array, Register size, Register scratch,
                 PushArrayOrder order = PushArrayOrder::kNormal);

  // Operand pointing to an external reference.
  // May emit code to set up the scratch register. The operand is
  // only guaranteed to be correct as long as the scratch register
  // isn't changed.
  // If the operand is used more than once, use a scratch register
  // that is guaranteed not to be clobbered.
  Operand ExternalReferenceAsOperand(ExternalReference reference,
                                     Register scratch);
  Operand ExternalReferenceAddressAsOperand(ExternalReference reference);
  Operand HeapObjectAsOperand(Handle<HeapObject> object);

  void LoadAddress(Register destination, ExternalReference source);

  void CompareRoot(Register with, RootIndex index);
  void CompareRoot(Register with, Register scratch, RootIndex index);

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  void Ret(int bytes_dropped, Register scratch);

// Instructions whose SSE and AVX take the same number and type of operands.
#define AVX_OP3_WITH_TYPE(macro_name, name, dst_type, src1_type, src2_type) \
  void macro_name(dst_type dst, src1_type src1, src2_type src2) {           \
    if (CpuFeatures::IsSupported(AVX)) {                                    \
      CpuFeatureScope scope(this, AVX);                                     \
      v##name(dst, src1, src2);                                             \
    } else {                                                                \
      name(dst, src1, src2);                                                \
    }                                                                       \
  }
  AVX_OP3_WITH_TYPE(Pshufhw, pshufhw, XMMRegister, Operand, uint8_t)
  AVX_OP3_WITH_TYPE(Pshufhw, pshufhw, XMMRegister, XMMRegister, uint8_t)
  AVX_OP3_WITH_TYPE(Pshuflw, pshuflw, XMMRegister, Operand, uint8_t)
  AVX_OP3_WITH_TYPE(Pshuflw, pshuflw, XMMRegister, XMMRegister, uint8_t)
  AVX_OP3_WITH_TYPE(Pshufd, pshufd, XMMRegister, Operand, uint8_t)
  AVX_OP3_WITH_TYPE(Pshufd, pshufd, XMMRegister, XMMRegister, uint8_t)
#undef AVX_OP3_WITH_TYPE

// Same as AVX_OP3_WITH_TYPE above but with SSE scope.
#define AVX_OP3_WITH_TYPE_SCOPE(macro_name, name, dst_type, src1_type, \
                                src2_type, sse_scope)                  \
  void macro_name(dst_type dst, src1_type src1, src2_type src2) {      \
    if (CpuFeatures::IsSupported(AVX)) {                               \
      CpuFeatureScope scope(this, AVX);                                \
      v##name(dst, src1, src2);                                        \
    } else {                                                           \
      CpuFeatureScope scope(this, sse_scope);                          \
      name(dst, src1, src2);                                           \
    }                                                                  \
  }
  AVX_OP3_WITH_TYPE_SCOPE(Pextrb, pextrb, Operand, XMMRegister, uint8_t, SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Pextrb, pextrb, Register, XMMRegister, uint8_t,
                          SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Pextrw, pextrw, Operand, XMMRegister, uint8_t, SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Pextrw, pextrw, Register, XMMRegister, uint8_t,
                          SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Extractps, extractps, Operand, XMMRegister, uint8_t,
                          SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Roundps, roundps, XMMRegister, XMMRegister,
                          RoundingMode, SSE4_1)
  AVX_OP3_WITH_TYPE_SCOPE(Roundpd, roundpd, XMMRegister, XMMRegister,
                          RoundingMode, SSE4_1)
#undef AVX_OP3_WITH_TYPE_SCOPE

// SSE/SSE2 instructions with AVX version.
#define AVX_OP2_WITH_TYPE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, src_type src) {                 \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope scope(this, AVX);                         \
      v##name(dst, src);                                        \
    } else {                                                    \
      name(dst, src);                                           \
    }                                                           \
  }

  AVX_OP2_WITH_TYPE(Movss, movss, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Movss, movss, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Movsd, movsd, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Movsd, movsd, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Rcpps, rcpps, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Rsqrtps, rsqrtps, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Movdqu, movdqu, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Movdqu, movdqu, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Movd, movd, XMMRegister, Register)
  AVX_OP2_WITH_TYPE(Movd, movd, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Movd, movd, Register, XMMRegister)
  AVX_OP2_WITH_TYPE(Movd, movd, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Cvtdq2ps, cvtdq2ps, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Cvtdq2ps, cvtdq2ps, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Cvtdq2pd, cvtdq2pd, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Cvtps2pd, cvtps2pd, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Cvtpd2ps, cvtpd2ps, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Cvttps2dq, cvttps2dq, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Sqrtps, sqrtps, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Sqrtpd, sqrtpd, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Sqrtpd, sqrtpd, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Movaps, movaps, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Movups, movups, XMMRegister, Operand)
  AVX_OP2_WITH_TYPE(Movups, movups, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Movups, movups, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Movapd, movapd, XMMRegister, XMMRegister)
  AVX_OP2_WITH_TYPE(Movapd, movapd, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Movupd, movupd, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Pmovmskb, pmovmskb, Register, XMMRegister)
  AVX_OP2_WITH_TYPE(Movmskpd, movmskpd, Register, XMMRegister)
  AVX_OP2_WITH_TYPE(Movmskps, movmskps, Register, XMMRegister)
  AVX_OP2_WITH_TYPE(Movlps, movlps, Operand, XMMRegister)
  AVX_OP2_WITH_TYPE(Movhps, movhps, Operand, XMMRegister)

#undef AVX_OP2_WITH_TYPE

// Only use these macros when non-destructive source of AVX version is not
// needed.
#define AVX_OP3_WITH_TYPE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, src_type src) {                 \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope scope(this, AVX);                         \
      v##name(dst, dst, src);                                   \
    } else {                                                    \
      name(dst, src);                                           \
    }                                                           \
  }
#define AVX_OP3_XO(macro_name, name)                            \
  AVX_OP3_WITH_TYPE(macro_name, name, XMMRegister, XMMRegister) \
  AVX_OP3_WITH_TYPE(macro_name, name, XMMRegister, Operand)

  AVX_OP3_XO(Packsswb, packsswb)
  AVX_OP3_XO(Packuswb, packuswb)
  AVX_OP3_XO(Paddusb, paddusb)
  AVX_OP3_XO(Pand, pand)
  AVX_OP3_XO(Pcmpeqb, pcmpeqb)
  AVX_OP3_XO(Pcmpeqw, pcmpeqw)
  AVX_OP3_XO(Pcmpeqd, pcmpeqd)
  AVX_OP3_XO(Por, por)
  AVX_OP3_XO(Psubb, psubb)
  AVX_OP3_XO(Psubw, psubw)
  AVX_OP3_XO(Psubd, psubd)
  AVX_OP3_XO(Psubq, psubq)
  AVX_OP3_XO(Punpcklbw, punpcklbw)
  AVX_OP3_XO(Punpckhbw, punpckhbw)
  AVX_OP3_XO(Punpckldq, punpckldq)
  AVX_OP3_XO(Punpcklqdq, punpcklqdq)
  AVX_OP3_XO(Pxor, pxor)
  AVX_OP3_XO(Andps, andps)
  AVX_OP3_XO(Andpd, andpd)
  AVX_OP3_XO(Xorps, xorps)
  AVX_OP3_XO(Xorpd, xorpd)
  AVX_OP3_XO(Sqrtss, sqrtss)
  AVX_OP3_XO(Sqrtsd, sqrtsd)
  AVX_OP3_XO(Orps, orps)
  AVX_OP3_XO(Orpd, orpd)
  AVX_OP3_XO(Andnpd, andnpd)
  AVX_OP3_XO(Pmullw, pmullw)
  AVX_OP3_WITH_TYPE(Movhlps, movhlps, XMMRegister, XMMRegister)
  AVX_OP3_WITH_TYPE(Psraw, psraw, XMMRegister, uint8_t)
  AVX_OP3_WITH_TYPE(Psrlq, psrlq, XMMRegister, uint8_t)

#undef AVX_OP3_XO
#undef AVX_OP3_WITH_TYPE

// Same as AVX_OP3_WITH_TYPE but supports a CpuFeatureScope
#define AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, dst_type, src_type, \
                                sse_scope)                            \
  void macro_name(dst_type dst, src_type src) {                       \
    if (CpuFeatures::IsSupported(AVX)) {                              \
      CpuFeatureScope scope(this, AVX);                               \
      v##name(dst, dst, src);                                         \
    } else if (CpuFeatures::IsSupported(sse_scope)) {                 \
      CpuFeatureScope scope(this, sse_scope);                         \
      name(dst, src);                                                 \
    }                                                                 \
  }
#define AVX_OP2_XO(macro_name, name, sse_scope)                       \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, XMMRegister, \
                          sse_scope)                                  \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, Operand, sse_scope)
  AVX_OP2_XO(Psignb, psignb, SSSE3)
  AVX_OP2_XO(Psignw, psignw, SSSE3)
  AVX_OP2_XO(Psignd, psignd, SSSE3)
  AVX_OP2_XO(Pcmpeqq, pcmpeqq, SSE4_1)
#undef AVX_OP2_XO
#undef AVX_OP2_WITH_TYPE_SCOPE

// Only use this macro when dst and src1 is the same in SSE case.
#define AVX_PACKED_OP3_WITH_TYPE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, dst_type src1, src_type src2) {        \
    if (CpuFeatures::IsSupported(AVX)) {                               \
      CpuFeatureScope scope(this, AVX);                                \
      v##name(dst, src1, src2);                                        \
    } else {                                                           \
      DCHECK_EQ(dst, src1);                                            \
      name(dst, src2);                                                 \
    }                                                                  \
  }
#define AVX_PACKED_OP3(macro_name, name)                               \
  AVX_PACKED_OP3_WITH_TYPE(macro_name, name, XMMRegister, XMMRegister) \
  AVX_PACKED_OP3_WITH_TYPE(macro_name, name, XMMRegister, Operand)

  AVX_PACKED_OP3(Unpcklps, unpcklps)
  AVX_PACKED_OP3(Andnps, andnps)
  AVX_PACKED_OP3(Addps, addps)
  AVX_PACKED_OP3(Addpd, addpd)
  AVX_PACKED_OP3(Subps, subps)
  AVX_PACKED_OP3(Subpd, subpd)
  AVX_PACKED_OP3(Mulps, mulps)
  AVX_PACKED_OP3(Mulpd, mulpd)
  AVX_PACKED_OP3(Divps, divps)
  AVX_PACKED_OP3(Divpd, divpd)
  AVX_PACKED_OP3(Cmpeqpd, cmpeqpd)
  AVX_PACKED_OP3(Cmpneqpd, cmpneqpd)
  AVX_PACKED_OP3(Cmpltpd, cmpltpd)
  AVX_PACKED_OP3(Cmpleps, cmpleps)
  AVX_PACKED_OP3(Cmplepd, cmplepd)
  AVX_PACKED_OP3(Minps, minps)
  AVX_PACKED_OP3(Minpd, minpd)
  AVX_PACKED_OP3(Maxps, maxps)
  AVX_PACKED_OP3(Maxpd, maxpd)
  AVX_PACKED_OP3(Cmpunordps, cmpunordps)
  AVX_PACKED_OP3(Cmpunordpd, cmpunordpd)
  AVX_PACKED_OP3(Psllw, psllw)
  AVX_PACKED_OP3(Pslld, pslld)
  AVX_PACKED_OP3(Psllq, psllq)
  AVX_PACKED_OP3(Psrlw, psrlw)
  AVX_PACKED_OP3(Psrld, psrld)
  AVX_PACKED_OP3(Psrlq, psrlq)
  AVX_PACKED_OP3(Psraw, psraw)
  AVX_PACKED_OP3(Psrad, psrad)
  AVX_PACKED_OP3(Paddd, paddd)
  AVX_PACKED_OP3(Paddq, paddq)
  AVX_PACKED_OP3(Psubd, psubd)
  AVX_PACKED_OP3(Psubq, psubq)
  AVX_PACKED_OP3(Pmuludq, pmuludq)
  AVX_PACKED_OP3(Pavgb, pavgb)
  AVX_PACKED_OP3(Pavgw, pavgw)
  AVX_PACKED_OP3(Pand, pand)
  AVX_PACKED_OP3(Pminub, pminub)
  AVX_PACKED_OP3(Pmaxub, pmaxub)
  AVX_PACKED_OP3(Paddusb, paddusb)
  AVX_PACKED_OP3(Psubusb, psubusb)
  AVX_PACKED_OP3(Pcmpgtb, pcmpgtb)
  AVX_PACKED_OP3(Pcmpeqb, pcmpeqb)
  AVX_PACKED_OP3(Paddb, paddb)
  AVX_PACKED_OP3(Paddsb, paddsb)
  AVX_PACKED_OP3(Psubb, psubb)
  AVX_PACKED_OP3(Psubsb, psubsb)

#undef AVX_PACKED_OP3

  AVX_PACKED_OP3_WITH_TYPE(Psllw, psllw, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Pslld, pslld, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psllq, psllq, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psrlw, psrlw, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psrld, psrld, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psrlq, psrlq, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psraw, psraw, XMMRegister, uint8_t)
  AVX_PACKED_OP3_WITH_TYPE(Psrad, psrad, XMMRegister, uint8_t)

#undef AVX_PACKED_OP3_WITH_TYPE

// Macro for instructions that have 2 operands for AVX version and 1 operand for
// SSE version. Will move src1 to dst if dst != src1.
#define AVX_OP3_WITH_MOVE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, dst_type src1, src_type src2) { \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope scope(this, AVX);                         \
      v##name(dst, src1, src2);                                 \
    } else {                                                    \
      if (dst != src1) {                                        \
        movaps(dst, src1);                                      \
      }                                                         \
      name(dst, src2);                                          \
    }                                                           \
  }
  AVX_OP3_WITH_MOVE(Cmpeqps, cmpeqps, XMMRegister, XMMRegister)
  AVX_OP3_WITH_MOVE(Movlps, movlps, XMMRegister, Operand)
  AVX_OP3_WITH_MOVE(Movhps, movhps, XMMRegister, Operand)
  AVX_OP3_WITH_MOVE(Pmaddwd, pmaddwd, XMMRegister, Operand)
#undef AVX_OP3_WITH_MOVE

// Non-SSE2 instructions.
#define AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, dst_type, src_type, \
                                sse_scope)                            \
  void macro_name(dst_type dst, src_type src) {                       \
    if (CpuFeatures::IsSupported(AVX)) {                              \
      CpuFeatureScope scope(this, AVX);                               \
      v##name(dst, src);                                              \
      return;                                                         \
    }                                                                 \
    if (CpuFeatures::IsSupported(sse_scope)) {                        \
      CpuFeatureScope scope(this, sse_scope);                         \
      name(dst, src);                                                 \
      return;                                                         \
    }                                                                 \
    UNREACHABLE();                                                    \
  }
#define AVX_OP2_XO_SSE3(macro_name, name)                                   \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, XMMRegister, SSE3) \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, Operand, SSE3)
  AVX_OP2_XO_SSE3(Movddup, movddup)
  AVX_OP2_WITH_TYPE_SCOPE(Movshdup, movshdup, XMMRegister, XMMRegister, SSE3)

#undef AVX_OP2_XO_SSE3

#define AVX_OP2_XO_SSSE3(macro_name, name)                                   \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, XMMRegister, SSSE3) \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, Operand, SSSE3)
  AVX_OP2_XO_SSSE3(Pabsb, pabsb)
  AVX_OP2_XO_SSSE3(Pabsw, pabsw)
  AVX_OP2_XO_SSSE3(Pabsd, pabsd)

#undef AVX_OP2_XO_SSE3

#define AVX_OP2_XO_SSE4(macro_name, name)                                     \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, XMMRegister, SSE4_1) \
  AVX_OP2_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, Operand, SSE4_1)

  AVX_OP2_XO_SSE4(Ptest, ptest)
  AVX_OP2_XO_SSE4(Pmovsxbw, pmovsxbw)
  AVX_OP2_XO_SSE4(Pmovsxwd, pmovsxwd)
  AVX_OP2_XO_SSE4(Pmovsxdq, pmovsxdq)
  AVX_OP2_XO_SSE4(Pmovzxbw, pmovzxbw)
  AVX_OP2_XO_SSE4(Pmovzxwd, pmovzxwd)
  AVX_OP2_XO_SSE4(Pmovzxdq, pmovzxdq)

#undef AVX_OP2_WITH_TYPE_SCOPE
#undef AVX_OP2_XO_SSE4

#define AVX_OP3_WITH_TYPE_SCOPE(macro_name, name, dst_type, src_type, \
                                sse_scope)                            \
  void macro_name(dst_type dst, dst_type src1, src_type src2) {       \
    if (CpuFeatures::IsSupported(AVX)) {                              \
      CpuFeatureScope scope(this, AVX);                               \
      v##name(dst, src1, src2);                                       \
      return;                                                         \
    }                                                                 \
    if (CpuFeatures::IsSupported(sse_scope)) {                        \
      CpuFeatureScope scope(this, sse_scope);                         \
      DCHECK_EQ(dst, src1);                                           \
      name(dst, src2);                                                \
      return;                                                         \
    }                                                                 \
    UNREACHABLE();                                                    \
  }
#define AVX_OP3_XO_SSE4(macro_name, name)                                     \
  AVX_OP3_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, XMMRegister, SSE4_1) \
  AVX_OP3_WITH_TYPE_SCOPE(macro_name, name, XMMRegister, Operand, SSE4_1)

  AVX_OP3_WITH_TYPE_SCOPE(Haddps, haddps, XMMRegister, Operand, SSE3)
  AVX_OP3_XO_SSE4(Pmaxsd, pmaxsd)
  AVX_OP3_XO_SSE4(Pminsb, pminsb)
  AVX_OP3_XO_SSE4(Pmaxsb, pmaxsb)
  AVX_OP3_XO_SSE4(Pcmpeqq, pcmpeqq)

#undef AVX_OP3_XO_SSE4
#undef AVX_OP3_WITH_TYPE_SCOPE

  void Pshufb(XMMRegister dst, XMMRegister src) { Pshufb(dst, dst, src); }
  void Pshufb(XMMRegister dst, Operand src) { Pshufb(dst, dst, src); }
  // Handles SSE and AVX. On SSE, moves src to dst if they are not equal.
  void Pshufb(XMMRegister dst, XMMRegister src, XMMRegister mask) {
    Pshufb(dst, src, Operand(mask));
  }
  void Pshufb(XMMRegister dst, XMMRegister src, Operand mask);

  void Pblendw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    Pblendw(dst, Operand(src), imm8);
  }
  void Pblendw(XMMRegister dst, Operand src, uint8_t imm8);

  void Palignr(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    Palignr(dst, Operand(src), imm8);
  }
  void Palignr(XMMRegister dst, Operand src, uint8_t imm8);

  void Pextrd(Register dst, XMMRegister src, uint8_t imm8);
  void Pinsrb(XMMRegister dst, Register src, int8_t imm8) {
    Pinsrb(dst, Operand(src), imm8);
  }
  void Pinsrb(XMMRegister dst, Operand src, int8_t imm8);
  // Moves src1 to dst if AVX is not supported.
  void Pinsrb(XMMRegister dst, XMMRegister src1, Operand src2, int8_t imm8);
  void Pinsrd(XMMRegister dst, Register src, uint8_t imm8) {
    Pinsrd(dst, Operand(src), imm8);
  }
  void Pinsrd(XMMRegister dst, Operand src, uint8_t imm8);
  // Moves src1 to dst if AVX is not supported.
  void Pinsrd(XMMRegister dst, XMMRegister src1, Operand src2, uint8_t imm8);
  void Pinsrw(XMMRegister dst, Register src, int8_t imm8) {
    Pinsrw(dst, Operand(src), imm8);
  }
  void Pinsrw(XMMRegister dst, Operand src, int8_t imm8);
  // Moves src1 to dst if AVX is not supported.
  void Pinsrw(XMMRegister dst, XMMRegister src1, Operand src2, int8_t imm8);
  void Vbroadcastss(XMMRegister dst, Operand src);

  // Shufps that will mov src1 into dst if AVX is not supported.
  void Shufps(XMMRegister dst, XMMRegister src1, XMMRegister src2,
              uint8_t imm8);

  // Expression support
  // cvtsi2sd instruction only writes to the low 64-bit of dst register, which
  // hinders register renaming and makes dependence chains longer. So we use
  // xorps to clear the dst register before cvtsi2sd to solve this issue.
  void Cvtsi2ss(XMMRegister dst, Register src) { Cvtsi2ss(dst, Operand(src)); }
  void Cvtsi2ss(XMMRegister dst, Operand src);
  void Cvtsi2sd(XMMRegister dst, Register src) { Cvtsi2sd(dst, Operand(src)); }
  void Cvtsi2sd(XMMRegister dst, Operand src);

  void Cvtui2ss(XMMRegister dst, Register src, Register tmp) {
    Cvtui2ss(dst, Operand(src), tmp);
  }
  void Cvtui2ss(XMMRegister dst, Operand src, Register tmp);
  void Cvttss2ui(Register dst, XMMRegister src, XMMRegister tmp) {
    Cvttss2ui(dst, Operand(src), tmp);
  }
  void Cvttss2ui(Register dst, Operand src, XMMRegister tmp);
  void Cvtui2sd(XMMRegister dst, Register src, Register scratch) {
    Cvtui2sd(dst, Operand(src), scratch);
  }
  void Cvtui2sd(XMMRegister dst, Operand src, Register scratch);
  void Cvttsd2ui(Register dst, XMMRegister src, XMMRegister tmp) {
    Cvttsd2ui(dst, Operand(src), tmp);
  }
  void Cvttsd2ui(Register dst, Operand src, XMMRegister tmp);

  // Handles SSE and AVX. On SSE, moves src to dst if they are not equal.
  void Pmulhrsw(XMMRegister dst, XMMRegister src1, XMMRegister src2);

  // These Wasm SIMD ops do not have direct lowerings on IA32. These
  // helpers are optimized to produce the fastest and smallest codegen.
  // Defined here to allow usage on both TurboFan and Liftoff.
  void I64x2ExtMul(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                   XMMRegister scratch, bool low, bool is_signed);
  // Requires that dst == src1 if AVX is not supported.
  void I32x4ExtMul(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                   XMMRegister scratch, bool low, bool is_signed);
  void I16x8ExtMulLow(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                      XMMRegister scratch, bool is_signed);
  void I16x8ExtMulHighS(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                        XMMRegister scratch);
  void I16x8ExtMulHighU(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                        XMMRegister scratch);
  // Requires dst == mask when AVX is not supported.
  void S128Select(XMMRegister dst, XMMRegister mask, XMMRegister src1,
                  XMMRegister src2, XMMRegister scratch);
  void I64x2SConvertI32x4High(XMMRegister dst, XMMRegister src);
  void I64x2UConvertI32x4High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  void I32x4SConvertI16x8High(XMMRegister dst, XMMRegister src);
  void I32x4UConvertI16x8High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  void I16x8SConvertI8x16High(XMMRegister dst, XMMRegister src);
  void I16x8UConvertI8x16High(XMMRegister dst, XMMRegister src,
                              XMMRegister scratch);
  void I16x8Q15MulRSatS(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                        XMMRegister scratch);
  void S128Store32Lane(Operand dst, XMMRegister src, uint8_t laneidx);
  void I8x16Popcnt(XMMRegister dst, XMMRegister src, XMMRegister tmp1,
                   XMMRegister tmp2, Register scratch);
  void F64x2ConvertLowI32x4U(XMMRegister dst, XMMRegister src, Register tmp);
  void I32x4TruncSatF64x2SZero(XMMRegister dst, XMMRegister src,
                               XMMRegister scratch, Register tmp);
  void I32x4TruncSatF64x2UZero(XMMRegister dst, XMMRegister src,
                               XMMRegister scratch, Register tmp);
  void I64x2Abs(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  void I64x2GtS(XMMRegister dst, XMMRegister src0, XMMRegister src1,
                XMMRegister scratch);
  void I64x2GeS(XMMRegister dst, XMMRegister src0, XMMRegister src1,
                XMMRegister scratch);
  void I16x8ExtAddPairwiseI8x16S(XMMRegister dst, XMMRegister src,
                                 XMMRegister tmp, Register scratch);
  void I16x8ExtAddPairwiseI8x16U(XMMRegister dst, XMMRegister src,
                                 Register scratch);
  void I32x4ExtAddPairwiseI16x8S(XMMRegister dst, XMMRegister src,
                                 Register scratch);
  void I32x4ExtAddPairwiseI16x8U(XMMRegister dst, XMMRegister src,
                                 XMMRegister tmp);
  void I8x16Swizzle(XMMRegister dst, XMMRegister src, XMMRegister mask,
                    XMMRegister scratch, Register tmp, bool omit_add = false);

  void Push(Register src) { push(src); }
  void Push(Operand src) { push(src); }
  void Push(Immediate value);
  void Push(Handle<HeapObject> handle) { push(Immediate(handle)); }
  void Push(Smi smi) { Push(Immediate(smi)); }
  void Push(XMMRegister src, Register scratch) {
    movd(scratch, src);
    push(scratch);
  }

  void Pop(Register dst) { pop(dst); }
  void Pop(Operand dst) { pop(dst); }
  void Pop(XMMRegister dst, Register scratch) {
    pop(scratch);
    movd(dst, scratch);
  }

  void SaveRegisters(RegList registers);
  void RestoreRegisters(RegList registers);

  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode);
  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode, Address wasm_target);
  void CallEphemeronKeyBarrier(Register object, Register address,
                               SaveFPRegsMode fp_mode);

  // Calculate how much stack space (in bytes) are required to store caller
  // registers excluding those specified in the arguments.
  int RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                      Register exclusion1 = no_reg,
                                      Register exclusion2 = no_reg,
                                      Register exclusion3 = no_reg) const;

  // PushCallerSaved and PopCallerSaved do not arrange the registers in any
  // particular order so they are not useful for calls that can cause a GC.
  // The caller can exclude up to 3 registers that do not need to be saved and
  // restored.

  // Push caller saved registers on the stack, and return the number of bytes
  // stack pointer is adjusted.
  int PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);
  // Restore caller saved registers from the stack, and return the number of
  // bytes stack pointer is adjusted.
  int PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                     Register exclusion2 = no_reg,
                     Register exclusion3 = no_reg);

  // Compute the start of the generated instruction stream from the current PC.
  // This is an alternative to embedding the {CodeObject} handle as a reference.
  void ComputeCodeStartAddress(Register dst);

  // TODO(860429): Remove remaining poisoning infrastructure on ia32.
  void ResetSpeculationPoisonRegister() { UNREACHABLE(); }

  // Control-flow integrity:

  // Define a function entrypoint. This doesn't emit any code for this
  // architecture, as control-flow integrity is not supported for it.
  void CodeEntry() {}
  // Define an exception handler.
  void ExceptionHandler() {}
  // Define an exception handler and bind a label.
  void BindExceptionHandler(Label* label) { bind(label); }

  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode, int builtin_index,
                           Address wasm_target);
};

// MacroAssembler implements a collection of frequently used macros.
class V8_EXPORT_PRIVATE MacroAssembler : public TurboAssembler {
 public:
  using TurboAssembler::TurboAssembler;

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int32_t x) {
    if (x == 0) {
      xor_(dst, dst);
    } else {
      mov(dst, Immediate(x));
    }
  }

  void PushRoot(RootIndex index);

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, RootIndex index, Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, RootIndex index, Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }

  // Checks if value is in range [lower_limit, higher_limit] using a single
  // comparison.
  void JumpIfIsInRange(Register value, unsigned lower_limit,
                       unsigned higher_limit, Register scratch,
                       Label* on_in_range,
                       Label::Distance near_jump = Label::kFar);

  // ---------------------------------------------------------------------------
  // GC Support
  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldOperand(reg, off).
  void RecordWriteField(
      Register object, int offset, Register value, Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // For page containing |object| mark region covering |address|
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. The address and value registers are clobbered by the
  // operation. RecordWrite filters out smis so it does not update the
  // write barrier if the value is a smi.
  void RecordWrite(
      Register object, Register address, Register value, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // Frame restart support
  void MaybeDropFrames();

  // Enter specific kind of exit frame. Expects the number of
  // arguments in register eax and sets up the number of arguments in
  // register edi and the pointer to the first argument in register
  // esi.
  void EnterExitFrame(int argc, bool save_doubles, StackFrame::Type frame_type);

  void EnterApiExitFrame(int argc, Register scratch);

  // Leave the current exit frame. Expects the return value in
  // register eax:edx (untouched) and the pointer to the first
  // argument in register esi (if pop_arguments == true).
  void LeaveExitFrame(bool save_doubles, bool pop_arguments = true);

  // Leave the current exit frame. Expects the return value in
  // register eax (untouched).
  void LeaveApiExitFrame();

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst);

  // Load a value from the native context with a given index.
  void LoadNativeContextSlot(Register dst, int index);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Invoke the JavaScript function code by either calling or jumping.

  void InvokeFunctionCode(Register function, Register new_target,
                          Register expected_parameter_count,
                          Register actual_parameter_count, InvokeFlag flag);

  // On function call, call into the debugger.
  // This may clobber ecx.
  void CallDebugOnFunctionCall(Register fun, Register new_target,
                               Register expected_parameter_count,
                               Register actual_parameter_count);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function, Register new_target,
                      Register actual_parameter_count, InvokeFlag flag);

  // Compare object type for heap object.
  // Incoming register is heap_object and outgoing register is map.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  void CmpInstanceType(Register map, InstanceType type);

  // Compare instance type ranges for a map (lower_limit and higher_limit
  // inclusive).
  //
  // Always use unsigned comparisons: below_equal for a positive
  // result.
  void CmpInstanceTypeRange(Register map, Register scratch,
                            InstanceType lower_limit,
                            InstanceType higher_limit);

  // Smi tagging support.
  void SmiTag(Register reg) {
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    add(reg, reg);
  }

  // Jump if register contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label,
                           Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(not_zero, not_smi_label, distance);
  }
  // Jump if the operand is not a smi.
  inline void JumpIfNotSmi(Operand value, Label* smi_label,
                           Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(not_zero, smi_label, distance);
  }

  template <typename Field>
  void DecodeField(Register reg) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> Field::kShift;
    if (shift != 0) {
      sar(reg, shift);
    }
    and_(reg, Immediate(mask));
  }

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);

  // Abort execution if argument is not a JSFunction, enabled via --debug-code.
  void AssertFunction(Register object, Register scratch);

  // Abort execution if argument is not a Constructor, enabled via --debug-code.
  void AssertConstructor(Register object);

  // Abort execution if argument is not a JSBoundFunction,
  // enabled via --debug-code.
  void AssertBoundFunction(Register object);

  // Abort execution if argument is not a JSGeneratorObject (or subclass),
  // enabled via --debug-code.
  void AssertGeneratorObject(Register object);

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  void AssertUndefinedOrAllocationSite(Register object, Register scratch);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link it into stack handler chain.
  void PushStackHandler(Register scratch);

  // Unlink the stack handler on top of the stack from the stack handler chain.
  void PopStackHandler(Register scratch);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs, save_doubles);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    CallRuntime(Runtime::FunctionForId(fid), num_arguments, save_doubles);
  }

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& ext,
                               bool builtin_exit_frame = false);

  // Generates a trampoline to jump to the off-heap instruction stream.
  void JumpToInstructionStream(Address entry);

  // ---------------------------------------------------------------------------
  // Utilities

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the esp register.
  void Drop(int element_count);

  // ---------------------------------------------------------------------------
  // In-place weak references.
  void LoadWeakValue(Register in_out, Label* target_if_cleared);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void IncrementCounter(StatsCounter* counter, int value, Register scratch);
  void DecrementCounter(StatsCounter* counter, int value, Register scratch);

  // ---------------------------------------------------------------------------
  // Stack limit utilities
  void CompareStackLimit(Register with, StackLimitKind kind);
  void StackOverflowCheck(Register num_args, Register scratch,
                          Label* stack_overflow, bool include_receiver = false);

  static int SafepointRegisterStackIndex(Register reg) {
    return SafepointRegisterStackIndex(reg.code());
  }

 private:
  // Helper functions for generating invokes.
  void InvokePrologue(Register expected_parameter_count,
                      Register actual_parameter_count, Label* done,
                      InvokeFlag flag);

  void EnterExitFramePrologue(StackFrame::Type frame_type, Register scratch);
  void EnterExitFrameEpilogue(int argc, bool save_doubles);

  void LeaveExitFrameEpilogue();

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class CommonFrame;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MacroAssembler);
};

// -----------------------------------------------------------------------------
// Static helper functions.

// Generate an Operand for loading a field from an object.
inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}

// Generate an Operand for loading an indexed field from an object.
inline Operand FieldOperand(Register object, Register index, ScaleFactor scale,
                            int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_IA32_MACRO_ASSEMBLER_IA32_H_
