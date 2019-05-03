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
    void say(__uint(32) va, __uint(32) vb);
};
__interface UserIndication {
    void gcd(__uint(32) v);
};

__module Gcd {
    UserRequest                     request;
    UserIndication                 *indication;
    __uint(1) busy;
    __uint(2) busy_delay;
    __uint(32) a, b;
    void request.say(__uint(32) va, __uint(32) vb) if (!busy) {
        a = va;
        b = vb;
        busy = 1;
    }
    __rule mod_rule if((a >= b) & (b != 0)) {
        a -= b;
    };
    __rule flip_rule if(a < b) {
        __uint(32) tmp = b;
        b = a;
        a = tmp;
    };
    __rule respond_rule if(b == 0) {
        busy = 0;
        indication->gcd(a);
   };
};
