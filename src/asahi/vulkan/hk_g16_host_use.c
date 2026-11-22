/*
 * Copyright 2026 Dylan Ironside
 * SPDX-License-Identifier: MIT
 */

#include "hk_g16_host_use.h"

/* Depth to which hk_g16_host_float_use() follows readers that may still fold
 * into a copy of a host result. Deeper chains are refused. */
#define HK_G16_HOST_FOLD_DEPTH 4

/* Whether `use` of a host-evaluated result is a float-typed source of an ALU
 * instruction that computes a new value from it. Branches, intrinsics and
 * untyped or integer sources see the bits as they are, NaN payload included.
 * So do the ops that return one of their operands unchanged: fneg and fabs
 * (sign bit only), fcsel, fcsel_gt and fcsel_ge (select an operand), fmin and
 * fmax (return an operand, so -0.0 against +0.0 and one NaN against another
 * survive), and fcanonicalize, which nir_opt_algebraic removes when fp32
 * denormals need not be flushed.
 *
 * The check runs before agx_compile_shader_nir(), whose nir_opt_algebraic can
 * still turn fmul(a, 1.0), fadd(a, -0.0), fadd(a, 0.0) under nsz and
 * ffma(a, 1.0, -0.0) into fcanonicalize(a) and then into a
 * (nir_opt_algebraic.py), so the reader's own readers then read the host
 * result directly. That needs every other source of the reader to be a
 * constant by then, which a source that varies across the draw never becomes.
 * When every other source is uniform, the reader's readers must pass this
 * check too. */
static bool hk_g16_host_float_use_depth(const nir_src *use, unsigned depth);

/* nir_opt_algebraic also turns fadd(fneg(a), fadd(a, b)) into b (inexact), so
 * `sum` = fadd(a, b), where b is the host result read through `use`, hands b
 * on unchanged to every reader of an fadd that adds fneg(a) to it. Those
 * readers must pass hk_g16_host_float_use() as well. */
static bool
hk_g16_host_cancel_ok(nir_alu_instr *sum, const nir_src *use, unsigned depth)
{
   const nir_def *a = sum->src[&sum->src[0].src == use ? 1 : 0].src.ssa;
   nir_foreach_use(next, &sum->def) {
      nir_instr *instr = nir_src_use_instr(next);
      if (instr->type != nir_instr_type_alu)
         continue;
      nir_alu_instr *outer = nir_instr_as_alu(instr);
      if (outer->op != nir_op_fadd)
         continue;
      const nir_def *x =
         outer->src[&outer->src[0].src == next ? 1 : 0].src.ssa;
      nir_alu_instr *neg = nir_def_as_alu_or_null(x);
      if (!neg || neg->op != nir_op_fneg || neg->src[0].src.ssa != a)
         continue;
      if (depth >= HK_G16_HOST_FOLD_DEPTH)
         return false;
      nir_foreach_use_including_if(reader, &outer->def) {
         if (!hk_g16_host_float_use_depth(reader, depth + 1))
            return false;
      }
   }
   return true;
}

static bool
hk_g16_host_float_use_depth(const nir_src *use, unsigned depth)
{
   if (nir_src_is_if(use))
      return false;
   nir_instr *user = nir_src_use_instr(use);
   if (user->type != nir_instr_type_alu)
      return false;
   nir_alu_instr *alu = nir_instr_as_alu(user);
   switch (alu->op) {
   case nir_op_fneg:
   case nir_op_fabs:
   case nir_op_fcsel:
   case nir_op_fcsel_gt:
   case nir_op_fcsel_ge:
   case nir_op_fmin:
   case nir_op_fmax:
   case nir_op_fcanonicalize:
      return false;
   default:
      break;
   }
   const nir_op_info *info = &nir_op_infos[alu->op];
   bool float_src = false;
   bool other_varies = false;
   for (unsigned i = 0; i < info->num_inputs; ++i) {
      if (&alu->src[i].src == use)
         float_src = nir_alu_type_get_base_type(info->input_types[i]) ==
                     nir_type_float;
      else if (alu->src[i].src.ssa->divergent)
         other_varies = true;
   }
   if (!float_src)
      return false;
   if (other_varies && alu->op == nir_op_fadd &&
       !hk_g16_host_cancel_ok(alu, use, depth))
      return false;
   if (other_varies || (alu->op != nir_op_fmul && alu->op != nir_op_fadd &&
                        alu->op != nir_op_ffma))
      return true;
   if (depth >= HK_G16_HOST_FOLD_DEPTH)
      return false;
   nir_foreach_use_including_if(next, &alu->def) {
      if (!hk_g16_host_float_use_depth(next, depth + 1))
         return false;
   }
   return true;
}

bool
hk_g16_host_float_use(const nir_src *use)
{
   return hk_g16_host_float_use_depth(use, 0);
}
