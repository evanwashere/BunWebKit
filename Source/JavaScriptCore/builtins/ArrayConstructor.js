/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

function of(/* items... */)
{
    "use strict";

    // !bun patch
    const len = @argumentCount();
    const extended = this !== @Array && @isConstructor(this);
    const arr = extended ? new this(len) : @newArrayWithSize(len);

    const unrollable = len <= 2 ** 30;

    if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, arguments[o]);

    else {
        const unrolled = len & ~3;

        for (let o = 0; o < unrolled; o += 4) {
            @putByValDirect(arr, o, arguments[o]);
            @putByValDirect(arr, o + 1, arguments[o + 1]);
            @putByValDirect(arr, o + 2, arguments[o + 2]);
            @putByValDirect(arr, o + 3, arguments[o + 3]);
        }

        for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, arguments[o]);
    }

    return (arr.length = len, arr);
    // @bun patch
}

function from(items /*, mapFn, thisArg */)
{
    "use strict";

    var mapFn = @argument(1);

    var thisArg;

    if (mapFn !== @undefined) {
        if (!@isCallable(mapFn))
            @throwTypeError("Array.from requires that the second argument, when provided, be a function");

        thisArg = @argument(2);
    }

    var arrayLike = @toObject(items, "Array.from requires an array-like object - not null or undefined");

    if (mapFn === @undefined) {
        var fastResult = @arrayFromFastFillWithUndefined(this, arrayLike);
        if (fastResult)
            return fastResult;
    }

    // !bun patch
    const iteratorMethod = items.@@iterator;

    if (!@isUndefinedOrNull(iteratorMethod)) {
        if (!@isCallable(iteratorMethod))
            @throwTypeError("Array.from requires that the property of the first argument, items[Symbol.iterator], when exists, be a function");

        const extended = this !== @Array && @isConstructor(this);

        let offset = 0;
        const arr = extended ? new this() : [];
        const iterator = iteratorMethod.@call(items);

        const wrapper = {
            @@iterator: function () { return iterator; }
        };

        if (mapFn === @undefined) {
            for (const value of wrapper) {
                if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
                @putByValDirect(arr, offset, value);

                offset++;
            }
        }

        else {
            if (thisArg === @undefined) {
                for (const value of wrapper) {
                    if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
                    @putByValDirect(arr, offset, mapFn(value, offset));

                    offset++;
                }
            }

            else {
                for (const value of wrapper) {
                    if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
                    @putByValDirect(arr, offset, mapFn.@call(thisArg, value, offset));

                    offset++;
                }
            }
        }

        return (arr.length = offset, arr);
    }
    // @bun patch

    // !bun patch
    const len = @toLength(arrayLike.length);
    const extended = this !== @Array && @isConstructor(this);
    const arr = extended ? new this(len) : @newArrayWithSize(len);
    
    const unrollable = len <= 2 ** 30;

    if (mapFn === @undefined) {
        if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, arrayLike[o]);

        else {
            const unrolled = len & ~3;

            for (let o = 0; o < unrolled; o += 4) {
                @putByValDirect(arr, o, arrayLike[o]);
                @putByValDirect(arr, o + 1, arrayLike[o + 1]);
                @putByValDirect(arr, o + 2, arrayLike[o + 2]);
                @putByValDirect(arr, o + 3, arrayLike[o + 3]);
            }

            for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, arrayLike[o]);
        }
    }

    else {
        if (thisArg === @undefined) {
            if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, mapFn(arrayLike[o], o));

            else {
                const unrolled = len & ~3;

                for (let o = 0; o < unrolled; o += 4) {
                    @putByValDirect(arr, o, mapFn(arrayLike[o], o));
                    @putByValDirect(arr, o + 1, mapFn(arrayLike[o + 1], o + 1));
                    @putByValDirect(arr, o + 2, mapFn(arrayLike[o + 2], o + 2));
                    @putByValDirect(arr, o + 3, mapFn(arrayLike[o + 3], o + 3));
                }

                for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, mapFn(arrayLike[o], o));
            }
        }

        else {
            if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, mapFn.@call(thisArg, arrayLike[o], o));

            else {
                const unrolled = len & ~3;

                for (let o = 0; o < unrolled; o += 4) {
                    @putByValDirect(arr, o, mapFn.@call(thisArg, arrayLike[o], o));
                    @putByValDirect(arr, o + 1, mapFn.@call(thisArg, arrayLike[o + 1], o + 1));
                    @putByValDirect(arr, o + 2, mapFn.@call(thisArg, arrayLike[o + 2], o + 2));
                    @putByValDirect(arr, o + 3, mapFn.@call(thisArg, arrayLike[o + 3], o + 3));
                }

                for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, mapFn.@call(thisArg, arrayLike[o], o));
            }
        }
    }

    return (arr.length = len, arr);
    // @bun patch
}

function isArray(array)
{
    "use strict";

    if (@isJSArray(array) || @isDerivedArray(array))
        return true;
    if (!@isProxyObject(array))
        return false;
    return @isArraySlow(array);
}

@linkTimeConstant
@visibility=PrivateRecursive
async function defaultAsyncFromAsyncIterator(iterator, mapFn, thisArg)
{
    "use strict";

    // !bun patch
    const extended = this !== @Array && @isConstructor(this);

    let offset = 0;
    const arr = extended ? new this() : [];

    const wrapper = {
        @@asyncIterator: function () { return iterator; }
    };

    if (mapFn === @undefined) {
        for await (const value of wrapper) {
            if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
            @putByValDirect(arr, offset, value);

            offset++;
        }
    }

    else {
        if (thisArg === @undefined) {
            for await (const value of wrapper) {
                if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
                @putByValDirect(arr, offset, await mapFn(value, offset));

                offset++;
            }
        }

        else {
            for await (const value of wrapper) {
                if (offset === @MAX_SAFE_INTEGER) @throwTypeError("Length exceeded the maximum array length");
                @putByValDirect(arr, offset, await mapFn.@call(thisArg, value, offset));

                offset++;
            }
        }
    }

    return (arr.length = offset, arr);
}

@linkTimeConstant
@visibility=PrivateRecursive
async function defaultAsyncFromAsyncArrayLike(asyncItems, mapFn, thisArg)
{
    "use strict";

    var arrayLike = @toObject(asyncItems, "Array.fromAsync requires an array-like object - not null or undefined");

    // !bun patch
    const len = @toLength(arrayLike.length);
    const extended = this !== @Array && @isConstructor(this);
    const arr = extended ? new this(len) : @newArrayWithSize(len);

    const unrollable = len <= 2 ** 30;

    if (mapFn === @undefined) {
        if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, await arrayLike[o]);

        else {
            const unrolled = len & ~3;

            for (let o = 0; o < unrolled; o += 4) {
                @putByValDirect(arr, o, await arrayLike[o]);
                @putByValDirect(arr, o + 1, await arrayLike[o + 1]);
                @putByValDirect(arr, o + 2, await arrayLike[o + 2]);
                @putByValDirect(arr, o + 3, await arrayLike[o + 3]);
            }

            for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, await arrayLike[o]);
        }
    }

    else {
        if (thisArg === @undefined) {
            if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, await mapFn(await arrayLike[o], o));

            else {
                const unrolled = len & ~3;

                for (let o = 0; o < unrolled; o += 4) {
                    @putByValDirect(arr, o, await mapFn(await arrayLike[o], o));
                    @putByValDirect(arr, o + 1, await mapFn(await arrayLike[o + 1], o + 1));
                    @putByValDirect(arr, o + 2, await mapFn(await arrayLike[o + 2], o + 2));
                    @putByValDirect(arr, o + 3, await mapFn(await arrayLike[o + 3], o + 3));
                }

                for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, await mapFn(await arrayLike[o], o));
            }
        }

        else {
            if (!unrollable) for (let o = 0; o < len; o++) @putByValDirect(arr, o, await mapFn.@call(thisArg, await arrayLike[o], o));
   
            else {
                const unrolled = len & ~3;
   
                for (let o = 0; o < unrolled; o += 4) {
                    @putByValDirect(arr, o, await mapFn.@call(thisArg, await arrayLike[o], o));
                    @putByValDirect(arr, o + 1, await mapFn.@call(thisArg, await arrayLike[o + 1], o + 1));
                    @putByValDirect(arr, o + 2, await mapFn.@call(thisArg, await arrayLike[o + 2], o + 2));
                    @putByValDirect(arr, o + 3, await mapFn.@call(thisArg, await arrayLike[o + 3], o + 3));
                }

                for (let o = unrolled; o < len; o++) @putByValDirect(arr, o, await mapFn.@call(thisArg, await arrayLike[o], o));
            }
        }
    }

    return (arr.length = len, arr);
    // @bun patch
}

function fromAsync(asyncItems  /*, mapFn, thisArg */)
{
    "use strict";

    try {
        var mapFn = @argument(1);

        var thisArg;

        if (mapFn !== @undefined) {
            if (!@isCallable(mapFn))
                @throwTypeError("Array.fromAsync requires that the second argument, when provided, be a function");

            thisArg = @argument(2);
        }

        var usingSyncIterator;
        var usingAsyncIterator = asyncItems.@@asyncIterator;
        if (!@isUndefinedOrNull(usingAsyncIterator)) {
            if (!@isCallable(usingAsyncIterator))
                @throwTypeError("Array.fromAsync requires that the property of the first argument, items[Symbol.asyncIterator], when exists, be a function");
        } else {
            usingSyncIterator = asyncItems.@@iterator;
            if (!@isUndefinedOrNull(usingSyncIterator)) {
                if (!@isCallable(usingSyncIterator))
                    @throwTypeError("Array.fromAsync requires that the property of the first argument, items[Symbol.iterator], when exists, be a function");
            }
        }

        if (!@isUndefinedOrNull(usingAsyncIterator))
            return @defaultAsyncFromAsyncIterator.@call(this, usingAsyncIterator.@call(asyncItems), mapFn, thisArg);

        if (!@isUndefinedOrNull(usingSyncIterator)) {
            var iterator = usingSyncIterator.@call(asyncItems);
            return @defaultAsyncFromAsyncIterator.@call(this, @createAsyncFromSyncIterator(iterator, iterator.next), mapFn, thisArg);
        }

        return @defaultAsyncFromAsyncArrayLike.@call(this, asyncItems, mapFn, thisArg);
    } catch (reason) {
        var promise = @newPromise();
        @rejectPromiseWithFirstResolvingFunctionCallCheck(promise, reason);
        return promise;
    }
}
