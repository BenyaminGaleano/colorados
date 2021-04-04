#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stdint.h>

uint32_t *pagedir_create (void);
void pagedir_destroy (uint32_t *pd);
bool pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool rw);
void *pagedir_get_page (uint32_t *pd, const void *upage);
void pagedir_clear_page (uint32_t *pd, void *upage);
bool pagedir_is_dirty (uint32_t *pd, const void *upage);
void pagedir_set_dirty (uint32_t *pd, const void *upage, bool dirty);
bool pagedir_is_accessed (uint32_t *pd, const void *upage);
void pagedir_set_accessed (uint32_t *pd, const void *upage, bool accessed);
void pagedir_activate (uint32_t *pd);

/** @colorados */
bool pagedir_in_swap (uint32_t *pd, void *upage);
void pagedir_set_swap (uint32_t *pd, void *upage, bool swap);
bool pagedir_zeored (uint32_t *pd, void *upage);
void pagedir_set_zeroed (uint32_t *pd, void *upage, bool zeroed);
bool pagedir_is_exe (uint32_t *pd, void *upage);
void pagedir_set_exe (uint32_t *pd, void *upage, bool exe);
bool pagedir_is_present (uint32_t *pd, void *upage);
bool pagedir_writes_access (uint32_t *pd, void *upage);
bool padedir_reinstall (uint32_t *pd, void *upage, void *kpage);
/** @colorados */

#endif /* userprog/pagedir.h */
