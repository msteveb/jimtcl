/*
 * Support for tty settings using termios
 *
 * (c) 2016 Steve Bennett <steveb@workware.net.au>
 *
 */

/* termios support is required */
#include <jim-tty.h>
#include <termios.h>

static const struct {
    unsigned baud;
    speed_t speed;
} baudtable[] = {
    { 0, B0 },
    { 50, B50 },
    { 75, B75 },
    { 110, B110 },
    { 134, B134 },
    { 150, B150 },
    { 200, B200 },
    { 300, B300 },
    { 600, B600 },
    { 1200, B1200 },
    { 1800, B1800 },
    { 2400, B2400 },
    { 4800, B4800 },
    { 9600, B9600 },
    { 19200, B19200 },
    { 38400, B38400 },
    { 57600, B57600 },
    { 115200, B115200 },
#ifdef B230400
    { 230400, B230400 },
#endif
#ifdef B460800
    { 460800, B460800 }
#endif
};

struct flag_name_map {
    const char *name;
    unsigned value;
};

static const struct flag_name_map parity_map[] = {
    { "none", 0 },
    { "even", PARENB },
    { "odd", PARENB | PARODD },
};
static const struct flag_name_map data_size_map[] = {
    { "5", CS5 },
    { "6", CS6 },
    { "7", CS7 },
    { "8", CS8 },
};
static const struct flag_name_map stop_size_map[] = {
    { "1", 0 },
    { "2", CSTOPB },
};
static const struct flag_name_map input_map[] = {
    { "raw", 0 },
    { "cooked", ICANON },
};
static const struct flag_name_map output_map[] = {
    { "raw", 0 },
    { "cooked", OPOST },
};

static const char * const tty_settings_names[] = {
    "baud",
    "data",
    "handshake",
    "input",
    "output",
    "parity",
    "stop",
    "vmin",
    "vtime",
    "echo",
    NULL
};

enum {
    OPT_BAUD,
    OPT_DATA,
    OPT_HANDSHAKE,
    OPT_INPUT,
    OPT_OUTPUT,
    OPT_PARITY,
    OPT_STOP,
    OPT_VMIN,
    OPT_VTIME,
    OPT_ECHO
};


#define ARRAYSIZE(A) (sizeof(A)/sizeof(*(A)))

/**
 * Search the flag/name map for an entry with the given name.
 * (Actually, just matching the first char)
 * Returns a pointer to the entry if found, or NULL if not.
 */
static const struct flag_name_map *flag_name_to_value(const struct flag_name_map *map, int len, const char *name)
{
    int i;

    for (i = 0; i < len; i++) {
        /* Only need to compare the first character since all names are unique in the first char */
        if (*name == *map[i].name) {
            return &map[i];
        }
    }
    return NULL;
}

/**
 * Search the flag/name map for an entry with the matching value.
 * Returns the corresponding name if found, or NULL if no match.
 */
static const char *flag_value_to_name(const struct flag_name_map *map, int len, unsigned value)
{
    int i;

    for (i = 0; i < len; i++) {
        if (value == map[i].value) {
            return map[i].name;
        }
    }
    return NULL;
}

/**
 * If 'str2' is not NULL, appends 'str1' and 'str2' to the list.
 * Otherwise does nothing.
 */
static void JimListAddPair(Jim_Interp *interp, Jim_Obj *listObjPtr, const char *str1, const char *str2)
{
    if (str2) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, str1, -1));
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, str2, -1));
    }
}

Jim_Obj *Jim_GetTtySettings(Jim_Interp *interp, int fd)
{
    struct termios tio;
    size_t i;
    const char *p;
    Jim_Obj *listObjPtr;
    speed_t speed;
    int baud;

    if (tcgetattr(fd, &tio) < 0) {
        return NULL;
    }

    listObjPtr = Jim_NewListObj(interp, NULL, 0);

    p = flag_value_to_name(parity_map, ARRAYSIZE(parity_map), tio.c_cflag & (PARENB | PARODD));
    JimListAddPair(interp, listObjPtr, "parity", p);
    p = flag_value_to_name(data_size_map, ARRAYSIZE(data_size_map), tio.c_cflag & CSIZE);
    JimListAddPair(interp, listObjPtr, "data", p);
    p = flag_value_to_name(stop_size_map, ARRAYSIZE(stop_size_map), tio.c_cflag & CSTOPB);
    JimListAddPair(interp, listObjPtr, "stop", p);
    if (tio.c_iflag & (IXON | IXOFF)) {
        p = "xonxoff";
    }
    else if (tio.c_cflag & CRTSCTS) {
        p = "rtscts";
    }
    else {
        p = "none";
    }
    JimListAddPair(interp, listObjPtr, "handshake", p);
    p = flag_value_to_name(input_map, ARRAYSIZE(input_map), tio.c_lflag & ICANON);
    JimListAddPair(interp, listObjPtr, "input", p);
    p = flag_value_to_name(output_map, ARRAYSIZE(output_map), tio.c_oflag & OPOST);
    JimListAddPair(interp, listObjPtr, "output", p);

    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, "vmin", -1));
    Jim_ListAppendElement(interp, listObjPtr, Jim_NewIntObj(interp, tio.c_cc[VMIN]));
    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, "vtime", -1));
    Jim_ListAppendElement(interp, listObjPtr, Jim_NewIntObj(interp, tio.c_cc[VTIME]));

    speed = cfgetispeed(&tio);
    baud = 0;
    for (i = 0; i < sizeof(baudtable) / sizeof(*baudtable); i++) {
        if (baudtable[i].speed == speed) {
            baud = baudtable[i].baud;
            break;
        }
    }
    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, "baud", -1));
    Jim_ListAppendElement(interp, listObjPtr, Jim_NewIntObj(interp, baud));

    return listObjPtr;
}

int Jim_SetTtySettings(Jim_Interp *interp, int fd, Jim_Obj *dictObjPtr)
{
    int len = Jim_ListLength(interp, dictObjPtr);
    int i;

    struct termios tio;

    if (tcgetattr(fd, &tio) < 0) {
        return -1;
    }

    for (i = 0; i < len; i += 2) {
        Jim_Obj *nameObj = Jim_ListGetIndex(interp, dictObjPtr, i);
        Jim_Obj *valueObj = Jim_ListGetIndex(interp, dictObjPtr, i + 1);
        int opt;
        const struct flag_name_map *p;
        long l;
        size_t j;

        if (Jim_GetEnum(interp, nameObj, tty_settings_names, &opt, "setting", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }

        switch (opt) {
            case OPT_BAUD:
                if (Jim_GetLong(interp, valueObj, &l) != JIM_OK) {
                    goto badvalue;
                }
                for (j = 0; j < ARRAYSIZE(baudtable); j++) {
                    if (baudtable[j].baud == l) {
                        break;
                    }
                }
                if (j == ARRAYSIZE(baudtable)) {
                    goto badvalue;
                }
                cfsetospeed(&tio, baudtable[j].speed);
                cfsetispeed(&tio, baudtable[j].speed);
                break;

            case OPT_PARITY:
                p = flag_name_to_value(parity_map, ARRAYSIZE(parity_map), Jim_String(valueObj));
                if (p == NULL) {
badvalue:
                    Jim_SetResultFormatted(interp, "bad value for %#s: %#s", nameObj, valueObj);
                    return JIM_ERR;
                }
                tio.c_cflag &= ~(PARENB | PARODD);
                tio.c_cflag |= p->value;
                break;

            case OPT_STOP:
                p = flag_name_to_value(stop_size_map, ARRAYSIZE(stop_size_map), Jim_String(valueObj));
                if (p == NULL) {
                    goto badvalue;
                }
                tio.c_cflag &= ~CSTOPB;
                tio.c_cflag |= p->value;
                break;

            case OPT_DATA:
                p = flag_name_to_value(data_size_map, ARRAYSIZE(data_size_map), Jim_String(valueObj));
                if (p == NULL) {
                    goto badvalue;
                }
                tio.c_cflag &= ~CSIZE;
                tio.c_cflag |= p->value;
                break;

            case OPT_HANDSHAKE:
                tio.c_iflag &= ~(IXON | IXOFF);
                tio.c_cflag &= ~(CRTSCTS);
                if (Jim_CompareStringImmediate(interp, valueObj, "xonxoff")) {
                    tio.c_iflag |= (IXON | IXOFF);
                }
                else if (Jim_CompareStringImmediate(interp, valueObj, "rtscts")) {
                    tio.c_cflag |= CRTSCTS;
                }
                else if (!Jim_CompareStringImmediate(interp, valueObj, "none")) {
                    goto badvalue;
                }
                break;

            case OPT_VMIN:
                if (Jim_GetLong(interp, valueObj, &l) != JIM_OK) {
                    goto badvalue;
                }
                tio.c_cc[VMIN] = l;
                break;

            case OPT_VTIME:
                if (Jim_GetLong(interp, valueObj, &l) != JIM_OK) {
                    goto badvalue;
                }
                tio.c_cc[VTIME] = l;
                break;

            case OPT_OUTPUT:
                p = flag_name_to_value(output_map, ARRAYSIZE(output_map), Jim_String(valueObj));
                if (p == NULL) {
                    goto badvalue;
                }
                tio.c_oflag &= ~OPOST;
                tio.c_oflag |= p->value;
                break;

            case OPT_INPUT:
                p = flag_name_to_value(input_map, ARRAYSIZE(input_map), Jim_String(valueObj));
                if (p == NULL) {
                    goto badvalue;
                }
                if (p->value) {
                    tio.c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL | ICANON | IEXTEN | ISIG | NOFLSH | TOSTOP);
                    tio.c_iflag |= ICRNL;
                }
                else {
                    tio.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | IEXTEN | ISIG | NOFLSH | TOSTOP);
                    tio.c_iflag &= ~ICRNL;
                }
                break;

            case OPT_ECHO:
                if (Jim_GetLong(interp, valueObj, &l) != JIM_OK) {
                    goto badvalue;
                }
                if (l) {
                    tio.c_lflag |= ECHO;
                }
                else {
                    tio.c_lflag &= ~ECHO;
                }
                break;

        }
    }

    if (tcsetattr(fd, TCSAFLUSH, &tio) < 0) {
        return -1;
    }
    return 0;
}
