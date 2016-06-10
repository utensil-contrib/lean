/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "library/trace.h"
#include "library/constants.h"
#include "library/unifier.h"
#include "library/kernel_serializer.h"
#include "library/let.h"
#include "library/typed_expr.h"
#include "library/choice.h"
#include "library/class.h"
#include "library/num.h"
#include "library/string.h"
#include "library/annotation.h"
#include "library/explicit.h"
#include "library/module.h"
#include "library/protected.h"
#include "library/private.h"
#include "library/scoped_ext.h"
#include "library/reducible.h"
#include "library/aliases.h"
#include "library/export_decl.h"
#include "library/coercion.h"
#include "library/io_state.h"
#include "library/idx_metavar.h"
#include "library/sorry.h"
#include "library/placeholder.h"
#include "library/light_lt_manager.h"
#include "library/print.h"
#include "library/fingerprint.h"
#include "library/util.h"
#include "library/pp_options.h"
#include "library/projection.h"
#include "library/normalize.h"
#include "library/abbreviation.h"
#include "library/relation_manager.h"
#include "library/user_recursors.h"
#include "library/composition_manager.h"
#include "library/noncomputable.h"
#include "library/aux_recursors.h"
#include "library/class_instance_resolution.h"
#include "library/type_context.h"
#include "library/local_context.h"
#include "library/metavar_context.h"
#include "library/congr_lemma_manager.h"
#include "library/app_builder.h"
#include "library/attribute_manager.h"
#include "library/fun_info_manager.h"
#include "library/unification_hint.h"
#include "library/defeq_simp_lemmas.h"
#include "library/defeq_simplifier.h"
#include "library/lazy_abstraction.h"

#include "library/old_converter.h"
#include "library/old_default_converter.h"
#include "library/old_type_checker.h"
#include "library/old_type_context.h"
#include "library/legacy_type_context.h"


namespace lean {
void initialize_library_core_module() {
    initialize_constants();
    initialize_trace();
}

void finalize_library_core_module() {
    finalize_trace();
    finalize_constants();
}

void initialize_library_module() {
// TODO(Leo): delete legacy....
    initialize_old_converter();
    initialize_old_default_converter();
    initialize_old_type_checker();
// ----------------------------

    initialize_local_context();
    initialize_metavar_context();
    initialize_attribute_manager();
    initialize_fingerprint();
    initialize_print();
    initialize_placeholder();
    initialize_idx_metavar();
    initialize_io_state();
    initialize_unifier();
    initialize_kernel_serializer();
    initialize_let();
    initialize_typed_expr();
    initialize_choice();
    initialize_string();
    initialize_num();
    initialize_annotation();
    initialize_explicit();
    initialize_module();
    initialize_protected();
    initialize_private();
    initialize_scoped_ext();
    initialize_reducible();
    initialize_aliases();
    initialize_export_decl();
    initialize_coercion();
    initialize_sorry();
    initialize_class();
    initialize_library_util();
    initialize_pp_options();
    initialize_projection();
    initialize_normalize();
    initialize_abbreviation();
    initialize_relation_manager();
    initialize_user_recursors();
    initialize_composition_manager();
    initialize_noncomputable();
    initialize_aux_recursors();
    initialize_class_instance_resolution();
    initialize_old_type_context();
    initialize_legacy_type_context();
    initialize_light_rule_set();
    initialize_congr_lemma_manager();
    initialize_app_builder();
    initialize_fun_info_manager();
    initialize_unification_hint();
    initialize_defeq_simp_lemmas();
    initialize_defeq_simplifier();
    initialize_type_context();
    initialize_lazy_abstraction();
}

void finalize_library_module() {
    finalize_lazy_abstraction();
    finalize_type_context();
    finalize_defeq_simplifier();
    finalize_defeq_simp_lemmas();
    finalize_unification_hint();
    finalize_fun_info_manager();
    finalize_app_builder();
    finalize_congr_lemma_manager();
    finalize_light_rule_set();
    finalize_legacy_type_context();
    finalize_old_type_context();
    finalize_class_instance_resolution();
    finalize_aux_recursors();
    finalize_noncomputable();
    finalize_composition_manager();
    finalize_user_recursors();
    finalize_relation_manager();
    finalize_abbreviation();
    finalize_normalize();
    finalize_projection();
    finalize_pp_options();
    finalize_library_util();
    finalize_class();
    finalize_sorry();
    finalize_coercion();
    finalize_export_decl();
    finalize_aliases();
    finalize_reducible();
    finalize_scoped_ext();
    finalize_private();
    finalize_protected();
    finalize_module();
    finalize_explicit();
    finalize_annotation();
    finalize_num();
    finalize_string();
    finalize_choice();
    finalize_typed_expr();
    finalize_let();
    finalize_kernel_serializer();
    finalize_unifier();
    finalize_io_state();
    finalize_idx_metavar();
    finalize_placeholder();
    finalize_print();
    finalize_fingerprint();
    finalize_attribute_manager();
    finalize_metavar_context();
    finalize_local_context();

// TODO(Leo): delete legacy....
    finalize_old_converter();
    finalize_old_default_converter();
    finalize_old_type_checker();
// -------------------
}
}
