/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.hardware.security.keymint;

/**
 * TagType classifies Tags in Tag.aidl into various groups of data.
 * @hide
 */
@VintfStability
@Backing(type="int")
enum TagType {
    /** Invalid type, used to designate a tag as uninitialized. */
    INVALID = 0 << 28,
    /** Enumeration value. */
    ENUM = 1 << 28,
    /** Repeatable enumeration value. */
    ENUM_REP = 2 << 28,
    /** 32-bit unsigned integer. */
    UINT = 3 << 28,
    /** Repeatable 32-bit unsigned integer. */
    UINT_REP = 4 << 28,
    /** 64-bit unsigned integer. */
    ULONG = 5 << 28,
    /** 64-bit unsigned integer representing a date and time, in milliseconds since 1 Jan 1970. */
    DATE = 6 << 28,
    /** Boolean.  If a tag with this type is present, the value is "true".  If absent, "false". */
    BOOL = 7 << 28,
    /** Byte string containing an arbitrary-length integer, big-endian ordering. */
    BIGNUM = 8 << 28,
    /** Byte string */
    BYTES = 9 << 28,
    /** Repeatable 64-bit unsigned integer */
    ULONG_REP = 10 << 28,
}
