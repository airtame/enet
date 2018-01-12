/** 
 @file  types.h
 @brief type definitions for ENet
*/
#ifndef __ENET_TYPES_H__
#define __ENET_TYPES_H__

#include <stdint.h>

/* For some reason enet defined its own fixed size ints.
   Use instead the standard ones, but keep the enet typedefs
   so we don't need to change all the code. */
typedef uint8_t enet_uint8;       /**< unsigned 8-bit type  */
typedef uint16_t enet_uint16;     /**< unsigned 16-bit type */
typedef uint32_t enet_uint32;     /**< unsigned 32-bit type */

#endif /* __ENET_TYPES_H__ */

