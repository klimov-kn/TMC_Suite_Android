/*
**
**  def_dest.     :  function for includer/definer
**
**  function      :
**                  dest_prep - destroy definer/includer : delete file VR_MAC_FILE_NAME
**  HEDER FILE'S  :
**                  <stdio.h>
**                  <bufm.h>
**
**  LIB           : NO
**
**  OVERLAY       : YES
**
*/

#include <stdio.h>
	 
#include <Typedef.h>

#include "Prepr1.h"



int dest_prep( void )
 {
   /* M15 (plan 5.1): the time file VR_MAC_FILE_NAME is not written any more, */
   /* so there is nothing to delete.                                           */
   return( 0 );
   }
