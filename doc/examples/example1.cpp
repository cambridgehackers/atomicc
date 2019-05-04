/* Copyright (c) 2019 The Connectal Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "atomicc.h"
__interface UserRequest {
    void say(__uint(32) v);
};
__interface UserIndication {
    void heard(__uint(32) v);
};

__module Example1 {
    UserRequest                     request;
    UserIndication                 *indication;
    __uint(1) busy;
    __uint(2) busy_delay;
    __uint(32) v_temp, v_delay;
    void request.say(__uint(32) v) if(!busy) {
        v_temp = v;
        busy = 1;
    }
    __rule delay_rule if(busy && busy_delay == 0) {
        busy_delay = 1;
        v_delay = v_temp;
    };
    __rule delay2_rule if(busy_delay == 1) {
        if (v_delay == 1)
           busy = 0;
        busy_delay = 2;
    };
    __rule respond_rule if(busy_delay == 2) {
        if (v_delay != 1)
           busy = 0;
        busy_delay = 0;
        indication->heard(v_delay);
   };
};
