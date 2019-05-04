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
    void say(__uint(32) va);
};

__module Order {
    UserRequest                     request;
    __uint(1) running;
    __uint(32) a, outA, outB, offset;
    void request.say(__uint(32) va) if (!running) {
        a = va;
        offset = 1;
        running = 1;
    }
    Order() {
    __rule A if (!__valid(request.say)) {
        outA = a + offset;
        if (running)
            a = a + 1;
    };
    __rule B if (!__valid(request.say)) {
        outB = a + offset;
        if (!running)
            a = 1;
    };
    __rule C if (!__valid(request.say)) {
        offset = offset + 1;
    };
    }
};

Order test;
