
/*
 * Copyright (C) Artem S. Povalyukhin
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NJS_ITERATOR_H_INCLUDED_
#define _NJS_ITERATOR_H_INCLUDED_


njs_int_t njs_array_iterator_create(njs_vm_t *vm, const njs_value_t *src,
    njs_value_t *dst, njs_object_enum_t kind);

njs_int_t njs_array_iterator_next(njs_vm_t *vm, njs_value_t *iterator,
    njs_value_t *retval);

njs_int_t njs_object_iterate(njs_vm_t *vm, njs_iterator_args_t *args,
    njs_iterator_handler_t handler, njs_value_t *retval);

njs_int_t njs_object_iterate_reverse(njs_vm_t *vm, njs_iterator_args_t *args,
    njs_iterator_handler_t handler, njs_value_t *retval);

njs_array_t *njs_iterator_to_array(njs_vm_t *vm, njs_value_t *iterator,
    njs_value_t *retval);


extern const njs_object_type_init_t  njs_iterator_type_init;
extern const njs_object_type_init_t  njs_array_iterator_type_init;

#endif /* _NJS_ITERATOR_H_INCLUDED_ */
