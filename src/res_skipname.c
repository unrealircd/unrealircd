#include <sys/types.h>
#include <stdio.h>
#include "nameser.h"
#include "common.h"

/*
 * Skip over a compressed domain name. Return the size or -1.
 */
int dn_skipname(u_char *comp_dn, u_char *eom)
{
	register u_char *cp;
	register int n;

	cp = comp_dn;
	while (cp < eom && (n = *cp++))
	{
		/*
		 * check for indirection
		 */
		switch (n & INDIR_MASK)
		{
		  case 0:	/* normal case, n == len */
			  cp += n;
			  continue;
		  default:	/* illegal type */
			  return (-1);
		  case INDIR_MASK:	/* indirection */
			  cp++;
		}
		break;
	}
	return (cp - comp_dn);
}
