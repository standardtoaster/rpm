/* Read all of the file associated with the descriptor.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <unistd.h>

#include "libelfP.h"
#include "common.h"


static void
set_address (Elf *elf, size_t offset)
	/*@modifies elf @*/
{
  if (elf->kind == ELF_K_AR)
    {
      Elf *child = elf->state.ar.children;

      while (child != NULL)
	{
	  if (child->map_address == NULL)
	    {
	      child->map_address = elf->map_address;
	      child->start_offset -= offset;
	      if (child->kind == ELF_K_AR)
		child->state.ar.offset -= offset;

	      set_address (child, offset);
	    }

	  child = child->next;
	}
    }
}


char *
internal_function
__libelf_readall (Elf *elf)
{
  /* Get the file.  */
  rwlock_wrlock (elf->lock);

  if (elf->map_address == NULL && unlikely (elf->fildes == -1))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      rwlock_unlock (elf->lock);
      return NULL;
    }

  /* If the file is not mmap'ed and not previously loaded, do it now.  */
  if (elf->map_address == NULL)
    {
      char *mem;

      /* If this is an archive and we have derived descriptors get the
	 locks for all of them.  */
      libelf_acquire_all (elf);

      /* Allocate all the memory we need.  */
      mem = (char *) malloc (elf->maximum_size);
      if (mem != NULL)
	{
	  /* Read the file content.  */
	  if ((size_t) pread (elf->fildes, mem, elf->maximum_size,
			      elf->start_offset) != elf->maximum_size)
	    {
	      /* Something went wrong.  */
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      free (mem);
	    }
	  else
	    {
	      /* Remember the address.  */
	      elf->map_address = mem;

	      /* Also remember that we allocated the memory.  */
	      elf->flags |= ELF_F_MALLOCED;

	      /* Propagate the information down to all children and
		 their children.  */
	      set_address (elf, elf->start_offset);

	      /* Correct the own offsets.  */
	      if (elf->kind == ELF_K_AR)
		elf->state.ar.offset -= elf->start_offset;
	      elf->start_offset = 0;
	    }
	}
      else
	__libelf_seterrno (ELF_E_NOMEM);

      /* Free the locks on the children.  */
      libelf_release_all (elf);
    }

  rwlock_unlock (elf->lock);

  return (char *) elf->map_address;
}
