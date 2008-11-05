/* Jim - POSIX extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2007 Uwe Klein Habertwedt <antirez@invece.org>
 *
 * $Id: jim-hwio.c,v 1.16 2008/03/10 21:49:29 macc Exp $
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/io.h>
#include <sys/mman.h>
#include <sched.h>

#define JIM_EXTENSION
#include "jim.h"

static void Jim_PosixSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}


static int Jim_HwIoTemplateCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    long longValue;
    
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?template?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[1], &longValue) != JIM_OK)
        return JIM_ERR;
// body
    return JIM_OK;
}
#define AREAINFOLEN 10
typedef struct areainfo {
	int len;
	int base;
	int flag;
	char name[20];
} AREAINFO;

AREAINFO areainfo[AREAINFOLEN];
#define NEW 0
#define FOUND 1
#define NOTFOUND -1
int placearea(const char *name,int lbase, int len, int flag)
{
	int i;
	int state = NOTFOUND;
	int firstempty = NOTFOUND;
	int numbase = 0;
	if (lbase==0) {
		sscanf(name,"%i",&numbase);
		// fprintf(stderr,"lb0 %s : %x \n",name,numbase);
	}
	for (i=0;(i<AREAINFOLEN);i++) {
		if (lbase && (areainfo[i].base == lbase)) {
			state = FOUND ; break;
		}
		if ((lbase==0) && areainfo[i].len && (strcmp(areainfo[i].name, name)==0)) {
			//fprintf(stderr,"found an entry %s @ %x \n",areainfo[i].name,areainfo[i].base);
			return areainfo[i].base;
		}
		if (numbase 
		   && (numbase >=   areainfo[i].base) 
		   && (numbase <  ( areainfo[i].base + areainfo[i].len))
		   )
			return(numbase);
		if (lbase && firstempty<0 && areainfo[i].flag == 0) {
			firstempty = i;
			//fprintf(stderr,"firstempty: %d\n",firstempty);
		}
	}
	if ((state==NOTFOUND)&&(firstempty>=0)) {
		i = firstempty;
		state = NEW;
	}
		switch (state) {
		case NEW:	strcpy(areainfo[i].name,name);
				//fprintf(stderr,"created an entry %s \n",areainfo[i].name);
		case FOUND:	areainfo[i].len = len;
		         	areainfo[i].base = lbase;
		         	areainfo[i].flag = flag;
				//fprintf(stderr,"refreshed an entry %s @ %x \n",areainfo[i].name,areainfo[i].base);
				break;
		default:	
				//fprintf(stderr,"notfound : entry %s \n",name);
				return lbase;
		}
	return areainfo[i].base;
}

int getbasefromID(const char *name) 
{
	return placearea(name,0,0,0);
}


static int Jim_HwIoAreaCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
	const char *op ;
	int strlen;
	const char *name = NULL;
	long offset = -1;
	long len = -1;
	long res;

	switch (argc) {
	case 5:	if (Jim_GetLong(interp, argv[4], &len) != JIM_OK) {
        		Jim_WrongNumArgs(interp, 1, argv, "invalid len");
                	return JIM_ERR;
		}
	case 4:	if (Jim_GetLong(interp, argv[3], &offset) != JIM_OK) {
        		Jim_WrongNumArgs(interp, 1, argv, "invalid offset");
                	return JIM_ERR;
		}
	case 3:	if (name = Jim_GetString(argv[2], &strlen),strlen == 0) {
                	return JIM_ERR;
		}
	case 2: if (op = Jim_GetString(argv[1], &strlen),strlen == 0) {
                        return JIM_ERR;
                }
		break;
	case 1:	op = "list";
		break;
	default:
        	Jim_WrongNumArgs(interp, 1, argv, "invalid offset");
                return JIM_ERR;
	}
	if (strcmp(op,"create") == 0 ) {
		if ( ioperm( offset, len, 1) == 0 ) {
			placearea(name,offset, len,1 );
			Jim_SetResult(interp,argv[2]);
		}
	} else if (strcmp(op,"delete") == 0 ) {
		res = ioperm( offset, len, 0); 
		placearea(name,offset, len, 0 );
		Jim_SetResult(interp, Jim_NewIntObj(interp, res));
	} else if (strcmp(op,"list") == 0 ) {
		res = 0;
		Jim_SetResult(interp, Jim_NewIntObj(interp, res));
	} else {
        	Jim_WrongNumArgs(interp, 1, argv, "unknown op");
                return JIM_ERR;
	}
	return JIM_OK;
}

static long base  = 0x300;
static long bcast = 0x30a;

static int Jim_HwIoBaseCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
	Jim_Obj *ret = NULL;

	switch (argc) {
	case 3:	if (Jim_GetLong(interp, argv[2], &bcast) != JIM_OK) {
        		Jim_WrongNumArgs(interp, 1, argv, "invalid bcast");
                	return JIM_ERR;
		}
	case 2:	if (Jim_GetLong(interp, argv[1], &base) != JIM_OK) {
        		Jim_WrongNumArgs(interp, 1, argv, "invalid base");
                	return JIM_ERR;
		}
	case 1:	
		break;
	default:
        	Jim_WrongNumArgs(interp, 1, argv, "invalid offset");
                return JIM_ERR;
	}
	ret = Jim_NewListObj(interp, 0, 0);
	Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, base));
	Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, bcast));
	Jim_SetResult(interp, ret);

	return JIM_OK;
}

// getIntsfromList retrieves a number of ints from a list (argv)
// first item must be a number consecutive elements can be numbers
// or '.' for repeat last number
// or '+' for increment last number by size
// or '-' for decrement last number by size

static int HwIo_getIntsfromList(Jim_Interp *interp, 
        Jim_Obj *arg, int size, int *lastval, int *pval)
{
	long tmpval;
	const char *strarg;
	int strlen = 0;

	if (Jim_GetLong(interp, arg, &tmpval) == JIM_OK) {
		*pval = tmpval; 
		//fprintf(stderr,"int conversion OK\n");
		return JIM_OK;
	} else if (lastval == 0) {
		//fprintf(stderr,"tmpval: %ld lastval %p \n",tmpval, lastval);
        	return JIM_ERR;
	} else if (strarg = Jim_GetString(arg, &strlen),strlen>0) {
		// fprintf(stderr,"lastval: %ld %p %s\n",tmpval, strarg,strarg);
		do {
			switch (*strarg) {
			case '-': *pval = *lastval - size; break;
			case '.': *pval = *lastval; break;
			case '+': *pval = *lastval + size; break;
			default:
				//fprintf(stderr,"extra  opt :%d %c %s\n",strlen,*strarg,strarg);
				return JIM_ERR;
			}
		} while (strarg++,*strarg);
		//fprintf(stderr,"pval: %d \n",*pval);
	} else {
		//fprintf(stderr,"else \n");
        	return JIM_ERR;
	}
	return JIM_OK;
}

#define MASK(width) ((1<<width)-1)
#define DATAOFFS 0
#define DATABITS 27
#define ADDROFFS DATABITS
#define ADDRBITS 5
#define DATAMASK  MASK(DATABITS)
#define ADDRMASK  MASK(ADDRBITS)
#define COMPOSE(addr,value) (((addr&ADDRMASK)<<ADDROFFS)|((value&DATAMASK)<<DATAOFFS))
#define STRIP(value) (value&DATAMASK)
#define ADDRREG 0x1f

static int Jim_HwIoRregCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
	int idx ;
	int lbase = base;
	int addrval;
	int *plast = NULL;
	int outvalue;

	Jim_Obj *ret = Jim_NewListObj(interp, 0, 0);


	for (idx=1;(idx < argc);idx++) {
		if (HwIo_getIntsfromList(interp, argv[idx],1,plast,&addrval) != JIM_OK) {
                        const char *board;
                        int strlen;
			// fprintf(stderr,"going for ID\n");
                        if ((idx == 1) && (board = Jim_GetString(argv[idx], &strlen))) {
				//fprintf(stderr,"cont for ID\n");
                                lbase = getbasefromID(board);
                                continue;
                        }
			Jim_WrongNumArgs(interp, 1, argv, "Format!");
			return JIM_ERR;
		}
		outvalue = COMPOSE(ADDRREG,addrval);
		// fprintf(stderr,"rreg lbase: %x addrval: %x outvalue: %08x\n",lbase,addrval,outvalue);
		inb(0x80);
		outl(outvalue,lbase);
#ifdef USE_USLEEP
		usleep(1000);
#else
		inb(0x80);
		inb(0x80);
		inb(0x80);
		inb(0x80);
		inb(0x80);
#endif
		Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, STRIP(inl(lbase))));
		inb(0x80);
		plast = &addrval;
	}
	Jim_SetResult(interp, ret);
	return JIM_OK;
}

#define RETisUINT  0
#define RETisINT   1
#define MAXBOARD   4
static int Jim_HwIoRregBlkCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
	long len = -1 ;
	int bcnt = 0;
	int lbase[MAXBOARD] = {base, 0, 0, 0 };
	long addr;
	long outvalue;
       	const char *board;
       	int strlen;
	int rettype=RETisUINT;
	int llength;
	int i,j,k;
	int swaplist[] = {0, 3, 2, 1};
	char swaplen = (sizeof(swaplist)/sizeof(int));
	long swapbuf[2*swaplen];

	Jim_Obj *ret = NULL;

	if ( argc < 3) {
		Jim_WrongNumArgs(interp, 1, argv, "wrong num args!");
		return JIM_ERR;
	}
	// check if the board spec is a list
	if ((argc>=4) && (Jim_ListLength(interp,argv[1],&llength),llength > 1)) {
		Jim_Obj* bobj;

		// fprintf(stderr,"larg length:%d \n",llength);
		for (i=0;(i<llength);i++) {
			Jim_ListIndex(interp,argv[1],i,&bobj,JIM_NONE);
			if ((board = Jim_GetString(bobj, &strlen))) {
				// fprintf(stderr,"larg Board: %s base %x \n",board,getbasefromID(board));
				lbase[bcnt++] = getbasefromID(board);
			}
		}
		argv++; argc--;
	}
	while ((argc > 3) && (board = Jim_GetString(argv[1], &strlen))) {
		if (strcmp("int",board)==0) {
			rettype = RETisINT;
			argv++; argc--;
			continue;
		}
		if (strcmp("noswap",board)==0) {
			swaplist[0] = 0;
			swaplist[1] = 1;
			swaplist[2] = 2;
			swaplist[3] = 3;
			argv++; argc--;
			continue;
		}
		if (sscanf(board,"swap:%i,%i,%i,%i",
			&swaplist[0],&swaplist[1],&swaplist[2],&swaplist[3]
		   )==4) {
			argv++; argc--;
			continue;
		}
		// fprintf(stderr,"marg Board: %s base %x \n",board,getbasefromID(board));
		//fprintf(stderr,"cont for ID\n");
		lbase[bcnt++] = getbasefromID(board);
		argv++; argc--;
	}
	if (bcnt > MAXBOARD) {
		Jim_WrongNumArgs(interp, 1, argv, "too many boards specified!");
		return JIM_ERR;
	}
	if (!bcnt) bcnt = 1;	// default base
	// get start
	if (Jim_GetLong(interp, argv[1], &addr) != JIM_OK) {
        	return JIM_ERR;
	}
        outvalue = COMPOSE(ADDRREG,addr);

	// get len
	if (Jim_GetLong(interp, argv[2], &len) != JIM_OK) {
        	return JIM_ERR;
	}
	len /= bcnt;	// fix for bcnt items per len--
	for (i=0;(i<bcnt);i++) {
        	// fprintf(stderr,"rregmult:%d lbase: %x addr: %lx outvalue %08lx (len: %lx )\n",bcnt,lbase[i],addr,outvalue,len);
		inb(0x80);
        	outl(outvalue,lbase[i]);
	}
#ifdef USE_USLEEP
		usleep(1000);
#else
		inb(0x80);
		inb(0x80);
		inb(0x80);
		inb(0x80);
		inb(0x80);
#endif
	ret = Jim_NewListObj(interp, 0, 0);
	switch (rettype) {
	case RETisUINT: 
		j = 0;
		while (len--) {
			for (i=0;(i<bcnt);i++,j++) {
				inb(0x80);
				swapbuf[j] = inl(lbase[i]);
			}
			if (j>=swaplen) {
				j -= swaplen;
				for (k=0;(k<swaplen);k++) {
					Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, swapbuf[j+swaplist[k]]));
				}
			}
		}
		inb(0x80);
		break;
	case RETisINT:
		j = 0;
		while (len--) {
			for (i=0;(i<bcnt);i++,j++) {
				inb(0x80);
				swapbuf[j] = inl(lbase[i]);
			}
			if (j>=swaplen) {
				j -= swaplen;
				for (k=0;(k<swaplen);k++) {
					Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, (int)swapbuf[j+swaplist[k]]));
				}
			}
		}
		inb(0x80);
	}
	Jim_SetResult(interp, ret);
	return JIM_OK;
}

static int Jim_HwIoWregCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
	int idx ;
	int lbase = base;
	int addr;
	int *plast = NULL;
	long val;
	long outvalue;
	int cnt = 0;
	

	for (idx=1;(idx < argc);idx++) {
		// fprintf(stderr,"argc %d idx %d\n",argc,idx);
		if (HwIo_getIntsfromList(interp, argv[idx],1,plast,&addr) != JIM_OK) {
			const char *board;
			int strlen;
			//fprintf(stderr,"going for ID\n");
			if ((idx == 1) && (board = Jim_GetString(argv[idx], &strlen))) {
				//fprintf(stderr,"cont for ID\n");
				lbase = getbasefromID(board);
				continue;
			}
			Jim_WrongNumArgs(interp, 1, argv, "Format!");
			return JIM_ERR;
		}
		plast = &addr;
		idx++;
		if (idx == argc)
			fprintf(stderr,"uneven number args @ %d\n",idx);
		if (Jim_GetLong(interp, argv[idx], &val) == JIM_OK) {
			outvalue = COMPOSE(addr,val);
			// fprintf(stderr,"wreg: lbase: %x addr: %x outvalue %08lx\n",lbase,addr,outvalue);
		inb(0x80);
			outl(outvalue,lbase);
		inb(0x80);
			// fprintf(stderr,"outb:%ld @ %d\n",outval,addrval);
			cnt++;
		} else 
			return JIM_ERR;
		//fprintf(stderr,"argc %d idx %d\n",argc,idx);
	}
	Jim_SetResult(interp, Jim_NewIntObj(interp, cnt));
	return JIM_OK;
}


#undef INOUT_SIZE
#undef INOUT_IN_OP
#undef INOUT_OUT_OP
#undef INOUT_IN_CMDNAME
#undef INOUT_OUT_CMDNAME
//end

#define INOUT_SIZE 1
#define INOUT_IN_OP inb
#define INOUT_OUT_OP outb
#define INOUT_IN_CMDNAME Jim_HwIoInbCommand
#define INOUT_OUT_CMDNAME Jim_HwIoOutbCommand
#include "jim-hwio.inoutblock.h"

#define INOUT_SIZE 2
#define INOUT_IN_OP inw
#define INOUT_OUT_OP outw
#define INOUT_IN_CMDNAME Jim_HwIoInwCommand
#define INOUT_OUT_CMDNAME Jim_HwIoOutwCommand
#include "jim-hwio.inoutblock.h"

#define INOUT_SIZE 4
#define INOUT_IN_OP inl
#define INOUT_OUT_OP outl
#define INOUT_IN_CMDNAME Jim_HwIoInlCommand
#define INOUT_OUT_CMDNAME Jim_HwIoOutlCommand
#include "jim-hwio.inoutblock.h"


static int Jim_HwIoMLockCommand( Jim_Interp *interp, int argc, 
	Jim_Obj *const *argv)
{
	const char *options[] = {
		"release", "current", "future", NULL
	};
	enum {RELEASE, CURRENT, FUTURE} ;
	int option;
	jim_wide start;
	jim_wide len;

	switch (argc) {
	case 3:	// Address Range
		if (Jim_GetWide(interp, argv[1], &start) != JIM_OK) {
                	Jim_WrongNumArgs(interp, 1, argv, "invalid Start");
                        return JIM_ERR;
                }
		if (Jim_GetWide(interp, argv[2], &len) != JIM_OK) {
                	Jim_WrongNumArgs(interp, 1, argv, "invalid Len");
                        return JIM_ERR;
                }
		if (mlock((void*)(long)start,len)) 
			goto tell_posix_error;
		return JIM_OK;
	case 2:
		if (Jim_GetEnum(interp, argv[1], options, &option, 
				"mlock type", JIM_ERRMSG) != JIM_OK)
        		return JIM_ERR;
		switch (option) {
		case RELEASE:
			if (munlockall())
				goto tell_posix_error;
			return JIM_OK;
		case CURRENT:
			if (mlockall(MCL_CURRENT))
				goto tell_posix_error;
			return JIM_OK;
		case FUTURE:
			if (mlockall(MCL_FUTURE))
				goto tell_posix_error;
			return JIM_OK;
		}
		break;
	case 1:
		return JIM_OK;
	default:
	        Jim_WrongNumArgs(interp, 1, argv, "?template?");
        	return JIM_ERR;
	}
tell_posix_error:
	Jim_PosixSetError(interp);
	return JIM_ERR;
}

#define SELF 0
static int Jim_HwIoSchedPriorityCommand( Jim_Interp *interp, int argc, 
	Jim_Obj *const *argv)
{
	struct sched_param sched_param;

	const char *policies[] = {
		"other", "rr", "fifo", NULL
	};
	enum { OTHER, RR, FIFO };
	int policy  = -1;
	int mypolicy;
	int min, max;
	jim_wide new;
	int newprio = 0;

	switch (argc) {
	case 3: if (Jim_GetWide(interp, argv[2], &new) != JIM_OK) {
                        Jim_WrongNumArgs(interp, 1, argv, "invalid Priority");
                        return JIM_ERR;
                }
		newprio = 1;
	case 2: if (Jim_GetEnum(interp, argv[1], policies, &policy,
			"Sched Priority Policy", JIM_ERRMSG) != JIM_OK)
			return JIM_ERR;
		switch (policy) {
		case OTHER:	policy = SCHED_OTHER 	; break;
		case RR:	policy = SCHED_RR 	; break;
		case FIFO:	policy = SCHED_FIFO 	; break;
		}
		min = sched_get_priority_min(policy);
		max = sched_get_priority_max(policy);
		//fprintf(stderr,"mypolicy: %d min:%d max:%d new:%d newprio:%d :\n",mypolicy,min,max,(int)new,newprio);
		if (( policy >= 0) && newprio && (new <0)) {
			sched_param.sched_priority = max + new;
			//fprintf(stderr,"priorel : %d \n",sched_param.sched_priority);
			if (sched_setscheduler(SELF, policy, &sched_param))
				goto tell_posix_error;
			return JIM_OK;
		}
		if (( policy >= 0) && newprio && (new >min)) {
			sched_param.sched_priority = new;
			//fprintf(stderr,"prioabs: %d \n",sched_param.sched_priority);
			if (sched_setscheduler(SELF, policy, &sched_param))
				goto tell_posix_error;
			return JIM_OK;
		}
		return JIM_ERR;
	case 1: {
		char buff[0x20];
		mypolicy = sched_getscheduler(SELF);
		switch (mypolicy) {
		case SCHED_OTHER: mypolicy = OTHER  ; break;
                case SCHED_RR:	  mypolicy = RR     ; break;
                case SCHED_FIFO:  mypolicy = FIFO   ; break;
		}
		if (sched_getparam(SELF, &sched_param))
			goto tell_posix_error;
		sprintf(buff,"%s %d",policies[mypolicy],sched_param.sched_priority);
		Jim_SetResultString(interp, buff, -1);
		return JIM_OK;
		}
	default:
		Jim_WrongNumArgs(interp, 1, argv, "?template?");
		return JIM_ERR;
	}


	sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 10;
	sched_setscheduler(0, SCHED_FIFO, &sched_param);

	return JIM_OK;
tell_posix_error:
	//fprintf(stderr,"telling posix error:");
        Jim_PosixSetError(interp);
        return JIM_ERR;
}

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "hwio", "0.5", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "hwio.area", Jim_HwIoAreaCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.base", Jim_HwIoBaseCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.inb" , Jim_HwIoInbCommand,  NULL, NULL);
    Jim_CreateCommand(interp, "hwio.outb", Jim_HwIoOutbCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.inw" , Jim_HwIoInwCommand,  NULL, NULL);
    Jim_CreateCommand(interp, "hwio.outw", Jim_HwIoOutwCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.inl" , Jim_HwIoInlCommand,  NULL, NULL);
    Jim_CreateCommand(interp, "hwio.outl", Jim_HwIoOutlCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.wreg", Jim_HwIoWregCommand, NULL, NULL);
    Jim_CreateCommand(interp, "hwio.rreg", Jim_HwIoRregCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.rblk", Jim_HwIoRregBlkCommand, NULL, NULL);

    Jim_CreateCommand(interp, "hwio.mlock", Jim_HwIoMLockCommand, NULL, NULL);
    Jim_CreateCommand(interp, "hwio.priority", Jim_HwIoSchedPriorityCommand, NULL, NULL);

    // get the delay port
    ioperm( 0x80, 1, 1);
    return JIM_OK;
}
