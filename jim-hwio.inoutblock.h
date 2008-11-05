/* Jim - POSIX extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2007 Uwe Klein Habertwedt <antirez@invece.org>
 *
 * $Id: jim-hwio.inoutblock.h,v 1.3 2008/02/11 17:09:39 macc Exp macc $
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

static int INOUT_IN_CMDNAME(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
	int idx ;
	int addrval;
	int *plast = NULL;

	Jim_Obj *ret = Jim_NewListObj(interp, 0, 0);


	for (idx=1;(idx < argc);idx++) {
		if (HwIo_getIntsfromList(interp, argv[idx],INOUT_SIZE,plast,&addrval) != JIM_OK) {
			Jim_WrongNumArgs(interp, 1, argv, "Format!");
			return JIM_ERR;
		}
				inb(0x80);
		Jim_ListAppendElement(interp,ret,Jim_NewIntObj(interp, INOUT_IN_OP(addrval)));
				inb(0x80);
		plast = &addrval;
	}
	Jim_SetResult(interp, ret);
	return JIM_OK;
}

static int INOUT_OUT_CMDNAME(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
	int idx ;
	int addrval;
	int *plast = NULL;
	long outval;
	int cnt = 0;

	for (idx=1;(idx < argc);idx++) {
		// fprintf(stderr,"argc %d idx %d\n",argc,idx);
		if (HwIo_getIntsfromList(interp, argv[idx],INOUT_SIZE,plast,&addrval) != JIM_OK) {
			Jim_WrongNumArgs(interp, 1, argv, "Format!");
			return JIM_ERR;
		}
		plast = &addrval;
		idx++;
		if (idx == argc)
			fprintf(stderr,"uneven number args @ %d\n",idx);
		if (Jim_GetLong(interp, argv[idx], &outval) == JIM_OK) {
				inb(0x80);
			INOUT_OUT_OP(outval,addrval);
				inb(0x80);
			// fprintf(stderr,"outb:%ld @ %d\n",outval,addrval);
			cnt++;
		} else 
			return JIM_ERR;
		// fprintf(stderr,"argc %d idx %d\n",argc,idx);
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
