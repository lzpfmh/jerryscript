/* Copyright 2014 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "jrt-bit-fields.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 */

static ecma_completion_value_t
ecma_builtin_dispatch_routine (ecma_builtin_id_t builtin_object_id,
                               ecma_magic_string_id_t builtin_routine_id,
                               ecma_value_t this_arg_value,
                               ecma_value_t arguments_list [],
                               ecma_length_t arguments_number);
static void ecma_instantiate_builtin (ecma_builtin_id_t id);

/**
 * Pointer to instances of built-in objects
 */
static ecma_object_t* ecma_builtin_objects [ECMA_BUILTIN_ID__COUNT];

/**
 * Check if passed object is the instance of specified built-in.
 */
bool
ecma_builtin_is (ecma_object_t *obj_p, /**< pointer to an object */
                 ecma_builtin_id_t builtin_id) /**< id of built-in to check on */
{
  JERRY_ASSERT (obj_p != NULL && !ecma_is_lexical_environment (obj_p));
  JERRY_ASSERT (builtin_id < ECMA_BUILTIN_ID__COUNT);

  if (unlikely (ecma_builtin_objects [builtin_id] == NULL))
  {
    ecma_instantiate_builtin (builtin_id);
  }

  return (obj_p == ecma_builtin_objects [builtin_id]);
} /* ecma_builtin_is */

/**
 * Get reference to specified built-in object
 *
 * @return pointer to the object's instance
 */
ecma_object_t*
ecma_builtin_get (ecma_builtin_id_t builtin_id) /**< id of built-in to check on */
{
  JERRY_ASSERT (builtin_id < ECMA_BUILTIN_ID__COUNT);

  if (unlikely (ecma_builtin_objects [builtin_id] == NULL))
  {
    ecma_instantiate_builtin (builtin_id);
  }

  ecma_ref_object (ecma_builtin_objects [builtin_id]);

  return ecma_builtin_objects [builtin_id];
} /* ecma_builtin_get */

/**
 * Initialize specified built-in object.
 *
 * Warning:
 *         the routine should be called only from ecma_init_builtins
 *
 * @return pointer to the object
 */
static ecma_object_t*
ecma_builtin_init_object (ecma_builtin_id_t obj_builtin_id, /**< built-in ID */
                          ecma_object_t* prototype_obj_p, /**< prototype object */
                          ecma_object_type_t obj_type, /**< object's type */
                          ecma_magic_string_id_t obj_class) /**< object's class */
{
  ecma_object_t *object_obj_p = ecma_create_object (prototype_obj_p, true, obj_type);

  ecma_property_t *class_prop_p = ecma_create_internal_property (object_obj_p,
                                                                 ECMA_INTERNAL_PROPERTY_CLASS);
  class_prop_p->u.internal_property.value = obj_class;

  ecma_property_t *built_in_id_prop_p = ecma_create_internal_property (object_obj_p,
                                                                       ECMA_INTERNAL_PROPERTY_BUILT_IN_ID);
  built_in_id_prop_p->u.internal_property.value = obj_builtin_id;

  ecma_set_object_is_builtin (object_obj_p, true);

  /** Initializing [[PrimitiveValue]] properties of built-in prototype objects */
  switch (obj_builtin_id)
  {
    case ECMA_BUILTIN_ID_STRING_PROTOTYPE:
    {
      ecma_string_t *prim_prop_str_value_p = ecma_get_magic_string (ECMA_MAGIC_STRING__EMPTY);

      ecma_property_t *prim_value_prop_p;
      prim_value_prop_p = ecma_create_internal_property (object_obj_p,
                                                         ECMA_INTERNAL_PROPERTY_PRIMITIVE_STRING_VALUE);
      ECMA_SET_POINTER (prim_value_prop_p->u.internal_property.value, prim_prop_str_value_p);
      break;
    }
    case ECMA_BUILTIN_ID_NUMBER_PROTOTYPE:
    {
      ecma_number_t *prim_prop_num_value_p = ecma_alloc_number ();
      *prim_prop_num_value_p = ECMA_NUMBER_ZERO;

      ecma_property_t *prim_value_prop_p;
      prim_value_prop_p = ecma_create_internal_property (object_obj_p,
                                                         ECMA_INTERNAL_PROPERTY_PRIMITIVE_NUMBER_VALUE);
      ECMA_SET_POINTER (prim_value_prop_p->u.internal_property.value, prim_prop_num_value_p);
      break;
    }
    case ECMA_BUILTIN_ID_BOOLEAN_PROTOTYPE:
    {
      ecma_property_t *prim_value_prop_p;
      prim_value_prop_p = ecma_create_internal_property (object_obj_p,
                                                         ECMA_INTERNAL_PROPERTY_PRIMITIVE_BOOLEAN_VALUE);
      prim_value_prop_p->u.internal_property.value = ECMA_SIMPLE_VALUE_FALSE;
      break;
    }

    default:
    {
      break;
    }
  }

  return object_obj_p;
} /* ecma_builtin_init_object */

/**
 * Initialize ECMA built-ins components
 */
void
ecma_init_builtins (void)
{
  for (ecma_builtin_id_t id = 0;
       id < ECMA_BUILTIN_ID__COUNT;
       id++)
  {
    ecma_builtin_objects [id] = NULL;
  }
} /* ecma_init_builtins */

/**
 * Instantiate specified ECMA built-in object
 */
static void
ecma_instantiate_builtin (ecma_builtin_id_t id) /**< built-in id */
{
  switch (id)
  {
#define CASE_BUILTIN(builtin_id, \
                     object_type, \
                     object_class, \
                     object_prototype_builtin_id, \
                     lowercase_name) \
    case ECMA_BUILTIN_ID_ ## builtin_id: \
    { \
      JERRY_ASSERT (ecma_builtin_objects [ECMA_BUILTIN_ID_ ## builtin_id] == NULL); \
      \
      ecma_object_t *prototype_obj_p; \
      if (object_prototype_builtin_id == ECMA_BUILTIN_ID__COUNT) \
      { \
        prototype_obj_p = NULL; \
      } \
      else \
      { \
        if (ecma_builtin_objects [object_prototype_builtin_id] == NULL) \
        { \
          ecma_instantiate_builtin (object_prototype_builtin_id); \
        } \
        prototype_obj_p = ecma_builtin_objects [object_prototype_builtin_id]; \
        JERRY_ASSERT (prototype_obj_p != NULL); \
      } \
      \
      ecma_object_t *builtin_obj_p =  ecma_builtin_init_object (ECMA_BUILTIN_ID_ ## builtin_id, \
                                                                prototype_obj_p, \
                                                                ECMA_OBJECT_ ## object_type, \
                                                                ECMA_MAGIC_STRING_ ## object_class); \
      ecma_builtin_objects [ECMA_BUILTIN_ID_ ## builtin_id] = builtin_obj_p; \
      \
      break; \
    }

    ECMA_BUILTIN_LIST (CASE_BUILTIN);

#undef CASE_BUILTIN

    default:
    {
      JERRY_UNREACHABLE ();
    }
  }
} /* ecma_instantiate_builtin */

/**
 * Finalize ECMA built-in objects
 */
void
ecma_finalize_builtins (void)
{
  for (ecma_builtin_id_t id = 0;
       id < ECMA_BUILTIN_ID__COUNT;
       id++)
  {
    if (ecma_builtin_objects [id] != NULL)
    {
      ecma_deref_object (ecma_builtin_objects [id]);

      ecma_builtin_objects [id] = NULL;
    }
  }
} /* ecma_finalize_builtins */

/**
 * If the property's name is one of built-in properties of the object
 * that is not instantiated yet, instantiate the property and
 * return pointer to the instantiated property.
 *
 * @return pointer property, if one was instantiated,
 *         NULL - otherwise.
 */
ecma_property_t*
ecma_builtin_try_to_instantiate_property (ecma_object_t *object_p, /**< object */
                                          ecma_string_t *string_p) /**< property's name */
{
  JERRY_ASSERT (ecma_get_object_is_builtin (object_p));

  ecma_property_t *built_in_id_prop_p = ecma_get_internal_property (object_p,
                                                                    ECMA_INTERNAL_PROPERTY_BUILT_IN_ID);
  ecma_builtin_id_t builtin_id = (ecma_builtin_id_t) built_in_id_prop_p->u.internal_property.value;

  JERRY_ASSERT (ecma_builtin_is (object_p, builtin_id));

  switch (builtin_id)
  {
#define TRY_TO_INSTANTIATE_PROPERTY(builtin_id, \
                                    object_type, \
                                    object_class, \
                                    object_prototype_builtin_id, \
                                    lowercase_name) \
    case ECMA_BUILTIN_ID_ ## builtin_id: \
    { \
      return ecma_builtin_ ## lowercase_name ## _try_to_instantiate_property (object_p, \
                                                                              string_p); \
    }

    ECMA_BUILTIN_LIST (TRY_TO_INSTANTIATE_PROPERTY)

#undef TRY_TO_INSTANTIATE_PROPERTY

    case ECMA_BUILTIN_ID__COUNT:
    {
      JERRY_UNREACHABLE ();
    }

    default:
    {
#ifdef CONFIG_ECMA_COMPACT_PROFILE
      JERRY_UNREACHABLE ();
#else /* CONFIG_ECMA_COMPACT_PROFILE */
      JERRY_UNIMPLEMENTED ("The built-in is not implemented.");
#endif /* !CONFIG_ECMA_COMPACT_PROFILE */
    }
  }

  JERRY_UNREACHABLE ();
} /* ecma_builtin_try_to_instantiate_property */

/**
 * Construct a Function object for specified built-in routine
 *
 * See also: ECMA-262 v5, 15
 *
 * @return pointer to constructed Function object
 */
ecma_object_t*
ecma_builtin_make_function_object_for_routine (ecma_builtin_id_t builtin_id, /**< identifier of built-in object
                                                                                  that initially contains property
                                                                                  with the routine */
                                               ecma_magic_string_id_t routine_id, /**< name of the built-in
                                                                                       object's routine property */
                                               ecma_number_t length_prop_num_value) /**< ecma-number - value
                                                                                         of 'length' property
                                                                                         of function object to create */
{
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);

  ecma_object_t *func_obj_p = ecma_create_object (prototype_obj_p, true, ECMA_OBJECT_TYPE_BUILT_IN_FUNCTION);

  ecma_deref_object (prototype_obj_p);

  ecma_set_object_is_builtin (func_obj_p, true);

  uint64_t packed_value = jrt_set_bit_field_value (0,
                                                   builtin_id,
                                                   ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_OBJECT_ID_POS,
                                                   ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_OBJECT_ID_WIDTH);
  packed_value = jrt_set_bit_field_value (packed_value,
                                          routine_id,
                                          ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_ROUTINE_ID_POS,
                                          ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_ROUTINE_ID_WIDTH);
  ecma_property_t *routine_id_prop_p = ecma_create_internal_property (func_obj_p,
                                                                      ECMA_INTERNAL_PROPERTY_BUILT_IN_ROUTINE_ID);

  JERRY_ASSERT ((uint32_t) packed_value == packed_value);
  routine_id_prop_p->u.internal_property.value = (uint32_t) packed_value;

  ecma_string_t* magic_string_length_p = ecma_get_magic_string (ECMA_MAGIC_STRING_LENGTH);
  ecma_property_t *len_prop_p = ecma_create_named_data_property (func_obj_p,
                                                                 magic_string_length_p,
                                                                 ECMA_PROPERTY_NOT_WRITABLE,
                                                                 ECMA_PROPERTY_NOT_ENUMERABLE,
                                                                 ECMA_PROPERTY_NOT_CONFIGURABLE);

  ecma_deref_ecma_string (magic_string_length_p);

  ecma_number_t* len_p = ecma_alloc_number ();
  *len_p = length_prop_num_value;

  len_prop_p->u.named_data_property.value = ecma_make_number_value (len_p);

  return func_obj_p;
} /* ecma_builtin_make_function_object_for_routine */

/**
 * Handle calling [[Call]] of built-in object
 *
 * @return completion-value
 */
ecma_completion_value_t
ecma_builtin_dispatch_call (ecma_object_t *obj_p, /**< built-in object */
                            ecma_value_t this_arg_value, /**< 'this' argument value */
                            ecma_value_t *arguments_list_p, /**< arguments list */
                            ecma_length_t arguments_list_len) /**< length of the arguments list */
{
  JERRY_ASSERT (ecma_get_object_is_builtin (obj_p));
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  if (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_BUILT_IN_FUNCTION)
  {
    ecma_property_t *id_prop_p = ecma_get_internal_property (obj_p,
                                                             ECMA_INTERNAL_PROPERTY_BUILT_IN_ROUTINE_ID);
    uint64_t packed_built_in_and_routine_id = id_prop_p->u.internal_property.value;

    uint64_t built_in_id_field = jrt_extract_bit_field (packed_built_in_and_routine_id,
                                                        ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_OBJECT_ID_POS,
                                                        ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_OBJECT_ID_WIDTH);
    JERRY_ASSERT (built_in_id_field < ECMA_BUILTIN_ID__COUNT);

    uint64_t routine_id_field = jrt_extract_bit_field (packed_built_in_and_routine_id,
                                                       ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_ROUTINE_ID_POS,
                                                       ECMA_BUILTIN_ROUTINE_ID_BUILT_IN_ROUTINE_ID_WIDTH);
    JERRY_ASSERT (routine_id_field < ECMA_MAGIC_STRING__COUNT);

    ecma_builtin_id_t built_in_id = (ecma_builtin_id_t) built_in_id_field;
    ecma_magic_string_id_t routine_id = (ecma_magic_string_id_t) routine_id_field;

    return ecma_builtin_dispatch_routine (built_in_id,
                                          routine_id,
                                          this_arg_value,
                                          arguments_list_p,
                                          arguments_list_len);
  }
  else
  {
    JERRY_ASSERT (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_FUNCTION);

    ecma_property_t *built_in_id_prop_p = ecma_get_internal_property (obj_p,
                                                                      ECMA_INTERNAL_PROPERTY_BUILT_IN_ID);
    ecma_builtin_id_t builtin_id = (ecma_builtin_id_t) built_in_id_prop_p->u.internal_property.value;

    JERRY_ASSERT (ecma_builtin_is (obj_p, builtin_id));

    switch (builtin_id)
    {
#define DISPATCH_CALL(builtin_id, \
                      object_type, \
                      object_class, \
                      object_prototype_builtin_id, \
                      lowercase_name) \
      case ECMA_BUILTIN_ID_ ## builtin_id: \
      { \
        if (ECMA_OBJECT_ ## object_type == ECMA_OBJECT_TYPE_FUNCTION) \
        { \
          return ecma_builtin_ ## lowercase_name ## _dispatch_call (arguments_list_p, \
                                                                    arguments_list_len); \
        } \
        else \
        { \
          JERRY_UNREACHABLE (); \
        } \
      }

      ECMA_BUILTIN_LIST (DISPATCH_CALL)

#undef DISPATCH_CALL

      case ECMA_BUILTIN_ID__COUNT:
      {
        JERRY_UNREACHABLE ();
      }

      default:
      {
#ifdef CONFIG_ECMA_COMPACT_PROFILE
        JERRY_UNREACHABLE ();
#else /* CONFIG_ECMA_COMPACT_PROFILE */
        JERRY_UNIMPLEMENTED ("The built-in is not implemented.");
#endif /* !CONFIG_ECMA_COMPACT_PROFILE */
      }
    }
  }

  JERRY_UNREACHABLE ();
} /* ecma_builtin_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in object
 *
 * @return completion-value
 */
ecma_completion_value_t
ecma_builtin_dispatch_construct (ecma_object_t *obj_p, /**< built-in object */
                                 ecma_value_t *arguments_list_p, /**< arguments list */
                                 ecma_length_t arguments_list_len) /**< length of the arguments list */
{
  JERRY_ASSERT (ecma_get_object_type (obj_p) == ECMA_OBJECT_TYPE_FUNCTION);

  JERRY_ASSERT (ecma_get_object_is_builtin (obj_p));
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_property_t *built_in_id_prop_p = ecma_get_internal_property (obj_p,
                                                                    ECMA_INTERNAL_PROPERTY_BUILT_IN_ID);
  ecma_builtin_id_t builtin_id = (ecma_builtin_id_t) built_in_id_prop_p->u.internal_property.value;

  JERRY_ASSERT (ecma_builtin_is (obj_p, builtin_id));

  switch (builtin_id)
  {
#define DISPATCH_CONSTRUCT(builtin_id, \
                           object_type, \
                           object_class, \
                           object_prototype_builtin_id, \
                           lowercase_name) \
    case ECMA_BUILTIN_ID_ ## builtin_id: \
      { \
        if (ECMA_OBJECT_ ## object_type == ECMA_OBJECT_TYPE_FUNCTION) \
        { \
          return ecma_builtin_ ## lowercase_name ## _dispatch_construct (arguments_list_p, \
                                                                         arguments_list_len); \
        } \
        else \
        { \
          JERRY_UNREACHABLE (); \
        } \
      }

    ECMA_BUILTIN_LIST (DISPATCH_CONSTRUCT)

#undef DISPATCH_CONSTRUCT

    case ECMA_BUILTIN_ID__COUNT:
    {
      JERRY_UNREACHABLE ();
    }

    default:
    {
#ifdef CONFIG_ECMA_COMPACT_PROFILE
      JERRY_UNREACHABLE ();
#else /* CONFIG_ECMA_COMPACT_PROFILE */
      JERRY_UNIMPLEMENTED ("The built-in is not implemented.");
#endif /* !CONFIG_ECMA_COMPACT_PROFILE */
    }
  }

  JERRY_UNREACHABLE ();
} /* ecma_builtin_dispatch_construct */

/**
 * Dispatcher of built-in routines
 *
 * @return completion-value
 *         Returned value must be freed with ecma_free_completion_value.
 */
static ecma_completion_value_t
ecma_builtin_dispatch_routine (ecma_builtin_id_t builtin_object_id, /**< built-in object' identifier */
                               ecma_magic_string_id_t builtin_routine_id, /**< name of the built-in object's
                                                                               routine property */
                               ecma_value_t this_arg_value, /**< 'this' argument value */
                               ecma_value_t arguments_list [], /**< list of arguments passed to routine */
                               ecma_length_t arguments_number) /**< length of arguments' list */
{
  switch (builtin_object_id)
  {
#define DISPATCH_ROUTINE(builtin_id, \
                         object_type, \
                         object_class, \
                         object_prototype_builtin_id, \
                         lowercase_name) \
    case ECMA_BUILTIN_ID_ ## builtin_id: \
      { \
        return ecma_builtin_ ## lowercase_name ## _dispatch_routine (builtin_routine_id, \
                                                                     this_arg_value, \
                                                                     arguments_list, \
                                                                     arguments_number); \
      }

    ECMA_BUILTIN_LIST (DISPATCH_ROUTINE)

#undef DISPATCH_ROUTINE

    case ECMA_BUILTIN_ID__COUNT:
    {
      JERRY_UNREACHABLE ();
    }

    default:
    {
#ifdef CONFIG_ECMA_COMPACT_PROFILE
      JERRY_UNREACHABLE ();
#else /* CONFIG_ECMA_COMPACT_PROFILE */
      JERRY_UNIMPLEMENTED ("The built-in is not implemented.");
#endif /* !CONFIG_ECMA_COMPACT_PROFILE */
    }
  }

  JERRY_UNREACHABLE ();
} /* ecma_builtin_dispatch_routine */

/**
 * Binary search for magic string identifier in array.
 *
 * Warning:
 *         array should be sorted in ascending order
 *
 * @return index of identifier, if it is contained in array,
 *         -1 - otherwise.
 */
int32_t
ecma_builtin_bin_search_for_magic_string_id_in_array (const ecma_magic_string_id_t ids[], /**< array to search in */
                                                      ecma_length_t array_length, /**< number of elements
                                                                                       in the array */
                                                      ecma_magic_string_id_t key) /**< value to search for */
{
#ifndef JERRY_NDEBUG
  /* For binary search the values should be sorted */
  for (ecma_length_t id_index = 1;
       id_index < array_length;
       id_index++)
  {
    JERRY_ASSERT (ids [id_index - 1] < ids [id_index]);
  }
#endif /* !JERRY_NDEBUG */

  int32_t min = 0;
  int32_t max = array_length - 1;
  
  while (min <= max)
  {
    int32_t mid = (min + max) / 2;

    if (ids[mid] == key)
    {
      return mid;
    }
    else if (ids[mid] > key)
    {
      max = mid - 1;
    }
    else
    {
      JERRY_ASSERT (ids[mid] < key);

      min = mid + 1;
    }
  }

  return -1;
} /* ecma_builtin_bin_search_for_magic_string_id_in_array */

/**
 * @}
 * @}
 */
