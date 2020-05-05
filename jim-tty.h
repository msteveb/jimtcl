#ifndef JIM_TTY_H
#define JIM_TTY_H

#include <jim.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the tty settings for the given file descriptor as a dictionary
 * with a zero reference count.
 *
 * Returns NULL and sets errno file descriptor is not a valid tty.
 */
Jim_Obj *Jim_GetTtySettings(Jim_Interp *interp, int fd);

/**
 * Sets the tty settings given in 'dictObjPtr'
 *
 * Returns JIM_OK if OK, JIM_ERR if any settings are invalid,
 * or -1 (and sets errno) if the file descriptor is not a valid tty.
 */
int Jim_SetTtySettings(Jim_Interp *interp, int fd, Jim_Obj *dictObjPtr);

/**
 * Return the tty modem control settings for the given file descriptor as a dictionary
 * with a zero reference count.
 *
 * Returns NULL and sets errno file descriptor is not a valid tty.
 */
Jim_Obj *Jim_GetTtyControlSettings(Jim_Interp *interp, int fd);

/**
 * Sets the tty modem control settings given in 'dictObjPtr'
 *
 * Returns JIM_OK if OK, JIM_ERR if any settings are invalid,
 * or -1 (and sets errno) if the file descriptor is not a valid tty.
 */
int Jim_SetTtyControlSettings(Jim_Interp *interp, int fd, Jim_Obj *dictObjPtr);

#ifdef __cplusplus
}
#endif

#endif
