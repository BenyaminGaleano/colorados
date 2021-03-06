#include "userprog/pagedir.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"

#ifdef VM
#include "vm/frame.h"
#endif


static uint32_t *active_pd (void);
static void invalidate_pagedir (uint32_t *);

/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint32_t *
pagedir_create (void) 
{
  uint32_t *pd = palloc_get_page (0);
  if (pd != NULL)
    memcpy (pd, init_page_dir, PGSIZE);
  return pd;
}

/* Destroys page directory PD, freeing all the pages it
   references. */
void
pagedir_destroy (uint32_t *pd) 
{
  uint32_t *pde;

  if (pd == NULL)
    return;

  ASSERT (pd != init_page_dir);
  for (pde = pd; pde < pd + pd_no (PHYS_BASE); pde++)
    if (*pde & PTE_P) 
      {
        uint32_t *pt = pde_get_pt (*pde);
        uint32_t *pte;
        
        for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
          if (*pte & PTE_P) 
            palloc_free_page (pte_get_page (*pte));
        palloc_free_page (pt);
      }
  palloc_free_page (pd);
}

/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  uint32_t *pt, *pde;

  ASSERT (pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
  ASSERT (!create || is_user_vaddr (vaddr));

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}

/* Adds a mapping in page directory PD from user virtual page
   UPAGE to the physical frame identified by kernel virtual
   address KPAGE.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   If WRITABLE is true, the new page is read/write;
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool
pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool writable)
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (pg_ofs (kpage) == 0);
  ASSERT (is_user_vaddr (upage));
  ASSERT (vtop (kpage) >> PTSHIFT < init_ram_pages);
  ASSERT (pd != init_page_dir);

  pte = lookup_page (pd, upage, true);

  if (pte != NULL) 
    {
      ASSERT ((*pte & PTE_P) == 0);
      *pte = pte_create_user (kpage, writable);

/** @colorados */
#ifdef VM
      struct frame *f = frame_lookup(kpage);
      if (f != NULL) {
        f->uaddr = upage;
        clock_add(f);
      }
#endif
/** @colorados */

      return true;
    }
  else
    return false;
}

/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void *
pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}

/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void
pagedir_clear_page (uint32_t *pd, void *upage) 
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  pte = lookup_page (pd, upage, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    {
      *pte &= ~PTE_P;
      invalidate_pagedir (pd);
    }
}

/* Returns true if the PTE for virtual page VPAGE in PD is dirty,
   that is, if the page has been modified since the PTE was
   installed.
   Returns false if PD contains no PTE for VPAGE. */
bool
pagedir_is_dirty (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_D) != 0;
}

/* Set the dirty bit to DIRTY in the PTE for virtual page VPAGE
   in PD. */
void
pagedir_set_dirty (uint32_t *pd, const void *vpage, bool dirty) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  if (pte != NULL) 
    {
      if (dirty)
        *pte |= PTE_D;
      else 
        {
          *pte &= ~(uint32_t) PTE_D;
          invalidate_pagedir (pd);
        }
    }
}

/* Returns true if the PTE for virtual page VPAGE in PD has been
   accessed recently, that is, between the time the PTE was
   installed and the last time it was cleared.  Returns false if
   PD contains no PTE for VPAGE. */
bool
pagedir_is_accessed (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_A) != 0;
}

/* Sets the accessed bit to ACCESSED in the PTE for virtual page
   VPAGE in PD. */
void
pagedir_set_accessed (uint32_t *pd, const void *vpage, bool accessed) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  if (pte != NULL) 
    {
      if (accessed)
        *pte |= PTE_A;
      else 
        {
          *pte &= ~(uint32_t) PTE_A; 
          invalidate_pagedir (pd);
        }
    }
}

/* Loads page directory PD into the CPU's page directory base
   register. */
void
pagedir_activate (uint32_t *pd) 
{
  if (pd == NULL)
    pd = init_page_dir;

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base
     Address of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (pd)) : "memory");
}

/* Returns the currently active page directory. */
static uint32_t *
active_pd (void) 
{
  /* Copy CR3, the page directory base register (PDBR), into
     `pd'.
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 3.7.5 "Base Address of the Page Directory". */
  uintptr_t pd;
  asm volatile ("movl %%cr3, %0" : "=r" (pd));
  return ptov (pd);
}

/* Seom page table changes can cause the CPU's translation
   lookaside buffer (TLB) to become out-of-sync with the page
   table.  When this happens, we have to "invalidate" the TLB by
   re-activating it.

   This function invalidates the TLB if PD is the active page
   directory.  (If PD is not active then its entries are not in
   the TLB, so there is no need to invalidate anything.) */
static void
invalidate_pagedir (uint32_t *pd) 
{
  if (active_pd () == pd) 
    {
      /* Re-activating PD clears the TLB.  See [IA32-v3a] 3.12
         "Translation Lookaside Buffers (TLBs)". */
      pagedir_activate (pd);
    } 
}


/** @colorados - CODE */

bool
pagedir_in_swap (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  return pte != NULL && (*pte & PTE_SWAP) != 0;
}


void
pagedir_set_swap (uint32_t *pd, void *upage, bool swap)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  if (pte != NULL) 
    {
      if (swap)
        *pte |= PTE_SWAP;
      else 
        *pte &= ~(uint32_t) PTE_SWAP;

      // need synchronization with TLB
      invalidate_pagedir (pd);
    }
}

bool
pagedir_is_exe (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  return pte != NULL && (*pte & PTE_EXE) != 0;
}


void
pagedir_set_exe (uint32_t *pd, void *upage, bool exe)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  if (pte != NULL) 
    {
      if (exe)
        *pte |= PTE_EXE;
      else 
        *pte &= ~(uint32_t) PTE_EXE;

      // need synchronization with TLB
      invalidate_pagedir (pd);
    }
}

bool
pagedir_is_present (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  return pte != NULL && (*pte & PTE_P) != 0;
}

bool
pagedir_zeroed (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);;
  return pte != NULL && (*pte & PTE_ZEROED) != 0;
}

void
pagedir_set_zeroed (uint32_t *pd, void *upage, bool zeroed)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  if (pte != NULL)
  {
    if (zeroed) {
      ASSERT(*pte & PTE_EXE);
      *pte |= PTE_ZEROED;
    } else
      *pte &= ~(uint32_t) PTE_ZEROED;
  }
}

bool
pagedir_writes_access (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  return pte != NULL && (*pte & PTE_W) != 0;
}


bool
pagedir_is_mmap (uint32_t *pd, void *upage)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  return pte != NULL && (*pte & PTE_EXE) == 0 && (*pte & PTE_ZEROED) != 0;
}


void
pagedir_set_mmap (uint32_t *pd, void *upage, bool mmap)
{
  uint32_t *pte = lookup_page (pd, upage, false);
  if (pte != NULL) 
    {
      ASSERT(!(*pte & PTE_EXE));
      if (mmap)
        *pte |= PTE_ZEROED;
      else 
        *pte &= ~(uint32_t) PTE_ZEROED;

      // need synchronization with TLB
      invalidate_pagedir (pd);
    }
}


// Reinstall the page that must be previously
// present and must be not present currently
bool
pagedir_reinstall (uint32_t *pd, void *upage, void *kpage)
{
  uint32_t *pte = lookup_page (pd, upage, false);

  ASSERT(!pagedir_is_present(pd, upage));
  ASSERT(pte != NULL);
  ASSERT(pg_ofs (upage) == 0);
  ASSERT(pg_ofs (kpage) == 0);

  //bool stack = pagedir_is_stack(pd, upage);
  bool exe = pagedir_is_exe(pd, upage);
  bool zeroed = pagedir_zeroed(pd, upage);
  bool swap = pagedir_in_swap(pd, upage);
  bool write = pagedir_writes_access(pd, upage);

  bool response = pagedir_set_page(pd, upage, kpage, write);

  pagedir_set_swap(pd, upage, swap);
  pagedir_set_mmap(pd, upage, swap);
  
  if (exe) {
    pagedir_set_exe(pd, upage, exe);
    pagedir_set_zeroed(pd, upage, zeroed);
  } else {
    pagedir_set_mmap(pd, upage, zeroed);
  }

  return response;
}
