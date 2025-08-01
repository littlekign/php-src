/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) Zend Technologies Ltd. (http://www.zend.com)           |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@php.net>                                 |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Dmitry Stogov <dmitry@php.net>                              |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_globals.h"
#include "zend_variables.h"
#include "zend_API.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "zend_weakrefs.h"
#include "zend_lazy_objects.h"

static zend_always_inline void _zend_object_std_init(zend_object *object, zend_class_entry *ce)
{
	GC_SET_REFCOUNT(object, 1);
	GC_TYPE_INFO(object) = GC_OBJECT;
	object->ce = ce;
	object->extra_flags = 0;
	object->handlers = ce->default_object_handlers;
	object->properties = NULL;
	zend_objects_store_put(object);
	if (UNEXPECTED(ce->ce_flags & ZEND_ACC_USE_GUARDS)) {
		zval *guard_value = object->properties_table + object->ce->default_properties_count;
		ZVAL_UNDEF(guard_value);
		Z_GUARD_P(guard_value) = 0;
	}
}

ZEND_API void ZEND_FASTCALL zend_object_std_init(zend_object *object, zend_class_entry *ce)
{
	_zend_object_std_init(object, ce);
}

void zend_object_dtor_dynamic_properties(zend_object *object)
{
	if (object->properties) {
		if (EXPECTED(!(GC_FLAGS(object->properties) & IS_ARRAY_IMMUTABLE))) {
			if (EXPECTED(GC_DELREF(object->properties) == 0)
					&& EXPECTED(GC_TYPE(object->properties) != IS_NULL)) {
				zend_array_destroy(object->properties);
			}
		}
	}
}

void zend_object_dtor_property(zend_object *object, zval *p)
{
	if (Z_REFCOUNTED_P(p)) {
		if (UNEXPECTED(Z_ISREF_P(p)) &&
				(ZEND_DEBUG || ZEND_REF_HAS_TYPE_SOURCES(Z_REF_P(p)))) {
			zend_property_info *prop_info = zend_get_property_info_for_slot_self(object, p);
			if (ZEND_TYPE_IS_SET(prop_info->type)) {
				ZEND_REF_DEL_TYPE_SOURCE(Z_REF_P(p), prop_info);
			}
		}
		i_zval_ptr_dtor(p);
	}
}

ZEND_API void zend_object_std_dtor(zend_object *object)
{
	zval *p, *end;

	if (UNEXPECTED(GC_FLAGS(object) & IS_OBJ_WEAKLY_REFERENCED)) {
		zend_weakrefs_notify(object);
	}

	if (UNEXPECTED(zend_object_is_lazy(object))) {
		zend_lazy_object_del_info(object);
	}

	zend_object_dtor_dynamic_properties(object);

	p = object->properties_table;
	if (EXPECTED(object->ce->default_properties_count)) {
		end = p + object->ce->default_properties_count;
		do {
			zend_object_dtor_property(object, p);
			p++;
		} while (p != end);
	}

	if (UNEXPECTED(object->ce->ce_flags & ZEND_ACC_USE_GUARDS)) {
		if (EXPECTED(Z_TYPE_P(p) == IS_STRING)) {
			zval_ptr_dtor_str(p);
		} else if (Z_TYPE_P(p) == IS_ARRAY) {
			HashTable *guards;

			guards = Z_ARRVAL_P(p);
			ZEND_ASSERT(guards != NULL);
			zend_hash_destroy(guards);
			FREE_HASHTABLE(guards);
		}
	}
}

ZEND_API void zend_objects_destroy_object(zend_object *object)
{
	zend_function *destructor = object->ce->destructor;

	if (destructor) {
		if (UNEXPECTED(zend_object_is_lazy(object))) {
			return;
		}

		zend_object *old_exception;
		const zend_op *old_opline_before_exception;

		if (destructor->common.fn_flags & (ZEND_ACC_PRIVATE|ZEND_ACC_PROTECTED)) {
			if (EG(current_execute_data)) {
				zend_class_entry *scope = zend_get_executed_scope();
				/* Ensure that if we're calling a protected or private function, we're allowed to do so. */
				ZEND_ASSERT(!(destructor->common.fn_flags & ZEND_ACC_PUBLIC));
				if (!zend_check_method_accessible(destructor, scope)) {
					zend_throw_error(NULL,
						"Call to %s %s::__destruct() from %s%s",
						zend_visibility_string(destructor->common.fn_flags), ZSTR_VAL(object->ce->name),
						scope ? "scope " : "global scope",
						scope ? ZSTR_VAL(scope->name) : ""
					);
					return;
				}
			} else {
				zend_error(E_WARNING,
					"Call to %s %s::__destruct() from global scope during shutdown ignored",
					zend_visibility_string(destructor->common.fn_flags), ZSTR_VAL(object->ce->name));
				return;
			}
		}

		GC_ADDREF(object);

		/* Make sure that destructors are protected from previously thrown exceptions.
		 * For example, if an exception was thrown in a function and when the function's
		 * local variable destruction results in a destructor being called.
		 */
		old_exception = NULL;
		if (EG(exception)) {
			if (EG(exception) == object) {
				zend_error_noreturn(E_CORE_ERROR, "Attempt to destruct pending exception");
			} else {
				if (EG(current_execute_data)
				 && EG(current_execute_data)->func
				 && ZEND_USER_CODE(EG(current_execute_data)->func->common.type)) {
					zend_rethrow_exception(EG(current_execute_data));
				}
				old_exception = EG(exception);
				old_opline_before_exception = EG(opline_before_exception);
				EG(exception) = NULL;
			}
		}

		zend_call_known_instance_method_with_0_params(destructor, object, NULL);

		if (old_exception) {
			EG(opline_before_exception) = old_opline_before_exception;
			if (EG(exception)) {
				zend_exception_set_previous(EG(exception), old_exception);
			} else {
				EG(exception) = old_exception;
			}
		}
		OBJ_RELEASE(object);
	}
}

ZEND_API zend_object* ZEND_FASTCALL zend_objects_new(zend_class_entry *ce)
{
	zend_object *object = emalloc(sizeof(zend_object) + zend_object_properties_size(ce));

	_zend_object_std_init(object, ce);
	return object;
}

ZEND_API void ZEND_FASTCALL zend_objects_clone_members(zend_object *new_object, zend_object *old_object)
{
	bool has_clone_method = old_object->ce->clone != NULL;

	if (old_object->ce->default_properties_count) {
		zval *src = old_object->properties_table;
		zval *dst = new_object->properties_table;
		zval *end = src + old_object->ce->default_properties_count;

		do {
			i_zval_ptr_dtor(dst);
			ZVAL_COPY_VALUE_PROP(dst, src);
			zval_add_ref(dst);
			if (has_clone_method) {
				/* Unconditionally add the IS_PROP_REINITABLE flag to avoid a potential cache miss of property_info */
				Z_PROP_FLAG_P(dst) |= IS_PROP_REINITABLE;
			}

			if (UNEXPECTED(Z_ISREF_P(dst)) &&
					(ZEND_DEBUG || ZEND_REF_HAS_TYPE_SOURCES(Z_REF_P(dst)))) {
				zend_property_info *prop_info = zend_get_property_info_for_slot_self(new_object, dst);
				if (ZEND_TYPE_IS_SET(prop_info->type)) {
					ZEND_REF_ADD_TYPE_SOURCE(Z_REF_P(dst), prop_info);
				}
			}
			src++;
			dst++;
		} while (src != end);
	} else if (old_object->properties && !has_clone_method) {
		/* fast copy */
		if (EXPECTED(old_object->handlers == &std_object_handlers)) {
			if (EXPECTED(!(GC_FLAGS(old_object->properties) & IS_ARRAY_IMMUTABLE))) {
				GC_ADDREF(old_object->properties);
			}
			new_object->properties = old_object->properties;
			return;
		}
	}

	if (old_object->properties &&
	    EXPECTED(zend_hash_num_elements(old_object->properties))) {
		zval *prop, new_prop;
		zend_ulong num_key;
		zend_string *key;

		if (!new_object->properties) {
			new_object->properties = zend_new_array(zend_hash_num_elements(old_object->properties));
			zend_hash_real_init_mixed(new_object->properties);
		} else {
			zend_hash_extend(new_object->properties, new_object->properties->nNumUsed + zend_hash_num_elements(old_object->properties), 0);
		}

		HT_FLAGS(new_object->properties) |=
			HT_FLAGS(old_object->properties) & HASH_FLAG_HAS_EMPTY_IND;

		ZEND_HASH_MAP_FOREACH_KEY_VAL(old_object->properties, num_key, key, prop) {
			if (Z_TYPE_P(prop) == IS_INDIRECT) {
				ZVAL_INDIRECT(&new_prop, new_object->properties_table + (Z_INDIRECT_P(prop) - old_object->properties_table));
			} else {
				ZVAL_COPY_VALUE(&new_prop, prop);
				zval_add_ref(&new_prop);
			}
			if (has_clone_method) {
				/* Unconditionally add the IS_PROP_REINITABLE flag to avoid a potential cache miss of property_info */
				Z_PROP_FLAG_P(&new_prop) |= IS_PROP_REINITABLE;
			}
			if (EXPECTED(key)) {
				_zend_hash_append(new_object->properties, key, &new_prop);
			} else {
				zend_hash_index_add_new(new_object->properties, num_key, &new_prop);
			}
		} ZEND_HASH_FOREACH_END();
	}

	if (has_clone_method) {
		zend_call_known_instance_method_with_0_params(new_object->ce->clone, new_object, NULL);

		if (ZEND_CLASS_HAS_READONLY_PROPS(new_object->ce)) {
			for (uint32_t i = 0; i < new_object->ce->default_properties_count; i++) {
				zval* prop = OBJ_PROP_NUM(new_object, i);
				/* Unconditionally remove the IS_PROP_REINITABLE flag to avoid a potential cache miss of property_info */
				Z_PROP_FLAG_P(prop) &= ~IS_PROP_REINITABLE;
			}
		}
	}
}

ZEND_API zend_object *zend_objects_clone_obj_with(zend_object *old_object, const zend_class_entry *scope, const HashTable *properties)
{
	zend_object *new_object = old_object->handlers->clone_obj(old_object);

	if (EXPECTED(!EG(exception))) {
		/* Unlock readonly properties once more. */
		if (ZEND_CLASS_HAS_READONLY_PROPS(new_object->ce)) {
			for (uint32_t i = 0; i < new_object->ce->default_properties_count; i++) {
				zval* prop = OBJ_PROP_NUM(new_object, i);
				Z_PROP_FLAG_P(prop) |= IS_PROP_REINITABLE;
			}
		}

		const zend_class_entry *old_scope = EG(fake_scope);

		EG(fake_scope) = scope;

		ZEND_HASH_FOREACH_KEY_VAL(properties, zend_ulong num_key, zend_string *key, zval *val) {
			if (UNEXPECTED(Z_ISREF_P(val))) {
				if (Z_REFCOUNT_P(val) == 1) {
					val = Z_REFVAL_P(val);
				} else {
					zend_throw_error(NULL, "Cannot assign by reference when cloning with updated properties");
					break;
				}
			}

			if (UNEXPECTED(key == NULL)) {
				key = zend_long_to_str(num_key);
				new_object->handlers->write_property(new_object, key, val, NULL);
				zend_string_release_ex(key, false);
			} else {
				new_object->handlers->write_property(new_object, key, val, NULL);
			}

			if (UNEXPECTED(EG(exception))) {
				break;
			}
		} ZEND_HASH_FOREACH_END();

		EG(fake_scope) = old_scope;
	}

	return new_object;
}

ZEND_API zend_object *zend_objects_clone_obj(zend_object *old_object)
{
	zend_object *new_object;

	if (UNEXPECTED(zend_object_is_lazy(old_object))) {
		return zend_lazy_object_clone(old_object);
	}

	/* assume that create isn't overwritten, so when clone depends on the
	 * overwritten one then it must itself be overwritten */
	new_object = zend_objects_new(old_object->ce);

	/* zend_objects_clone_members() expect the properties to be initialized. */
	if (new_object->ce->default_properties_count) {
		zval *p = new_object->properties_table;
		zval *end = p + new_object->ce->default_properties_count;
		do {
			ZVAL_UNDEF(p);
			p++;
		} while (p != end);
	}

	zend_objects_clone_members(new_object, old_object);

	return new_object;
}
