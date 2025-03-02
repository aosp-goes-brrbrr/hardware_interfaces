/*
 * Copyright (C) 2021 The Android Open Source Project
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
 * ProtectedData contains the encrypted BCC and the ephemeral MAC key used to
 * authenticate the keysToSign (see keysToSignMac output argument).
 * @hide
 */
@VintfStability
parcelable ProtectedData {
    /**
     * ProtectedData is a COSE_Encrypt structure, specified by the following CDDL
     *
     *     ProtectedData = [               // COSE_Encrypt
     *         protected: bstr .cbor {
     *             1 : 3                   // Algorithm : AES-GCM 256
     *         },
     *         unprotected: {
     *             5 : bstr .size 12       // IV
     *         },
     *         ciphertext: bstr,           // AES-GCM-256(K, .cbor ProtectedDataPayload)
     *         recipients : [
     *             [                       // COSE_Recipient
     *                 protected : bstr .cbor {
     *                     1 : -25         // Algorithm : ECDH-ES + HKDF-256
     *                 },
     *                 unprotected : {
     *                     -1 : {          // COSE_Key
     *                         1 : 1,      // Key type : Octet Key Pair
     *                         -1 : 4,     // Curve : X25519
     *                         -2 : bstr   // Sender X25519 public key
     *                     }
     *                     4 : bstr,       // KID : EEK ID
     *                 },
     *                 ciphertext : nil
     *             ]
     *         ]
     *     ]
     *
     *     K = HKDF-256(ECDH(EEK_pub, Ephemeral_priv), Context)
     *
     *     Context = [                     // COSE_KDF_Context
     *         AlgorithmID : 3             // AES-GCM 256
     *         PartyUInfo : [
     *             identity : bstr "client"
     *             nonce : bstr .size 0,
     *             other : bstr            // Ephemeral pubkey
     *         ],
     *         PartyVInfo : [
     *             identity : bstr "server",
     *             nonce : bstr .size 0,
     *             other : bstr            // EEK pubkey
     *         ],
     *         SuppPubInfo : [
     *             128,                    // Output key length
     *             protected : bstr .size 0
     *         ]
     *     ]
     *
     *     ProtectedDataPayload [
     *         SignedMac,
     *         Bcc,
     *     ]
     *
     *     SignedMac = [                       // COSE_Sign1
     *         bstr .cbor {                    // Protected params
     *             1 : -8,                     // Algorithm : EdDSA
     *         },
     *         { },                            // Unprotected params
     *         bstr .size 32,                  // MAC key
     *         bstr PureEd25519(DK_priv, .cbor SignedMac_structure)
     *     ]
     *
     *     SignedMac_structure = [
     *         "Signature1",
     *         bstr .cbor {                    // Protected params
     *             1 : -8,                     // Algorithm : EdDSA
     *         },
     *         bstr .cbor SignedMacAad
     *         bstr .size 32                   // MAC key
     *     ]
     *
     *     SignedMacAad = [
     *         challenge : bstr,
     *         DeviceInfo
     *     ]
     *
     *     Bcc = [
     *         PubKey,                        // DK_pub
     *         + BccEntry,                    // Root -> leaf (KM_pub)
     *     ]
     *
     *     BccPayload = {                     // CWT
     *         1 : tstr,                      // Issuer
     *         2 : tstr,                      // Subject
     *         // See the Open Profile for DICE for details on these fields.
     *         ? -4670545 : bstr,             // Code Hash
     *         ? -4670546 : bstr,             // Code Descriptor
     *         ? -4670547 : bstr,             // Configuration Hash
     *         ? -4670548 : bstr .cbor {      // Configuration Descriptor
     *             ? -70002 : tstr,           // Component name
     *             ? -70003 : int,            // Firmware version
     *             ? -70004 : null,           // Resettable
     *         },
     *         ? -4670549 : bstr,             // Authority Hash
     *         ? -4670550 : bstr,             // Authority Descriptor
     *         ? -4670551 : bstr,             // Mode
     *         -4670552 : bstr .cbor PubKey   // Subject Public Key
     *         -4670553 : bstr                // Key Usage
     *     }
     *
     *     BccEntry = [                       // COSE_Sign1
     *         protected: bstr .cbor {
     *             1 : -8,                    // Algorithm : EdDSA
     *         },
     *         unprotected: { },
     *         payload: bstr .cbor BccPayload,
     *         // First entry in the chain is signed by DK_pub, the others are each signed by their
     *         // immediate predecessor.  See RFC 8032 for signature representation.
     *         signature: bstr .cbor PureEd25519(SigningKey, bstr .cbor BccEntryInput)
     *     ]
     *
     *     PubKey = {                         // COSE_Key
     *         1 : 1,                         // Key type : octet key pair
     *         3 : -8,                        // Algorithm : EdDSA
     *         4 : 2,                         // Ops: Verify
     *         -1 : 6,                        // Curve : Ed25519
     *         -2 : bstr                      // X coordinate, little-endian
     *     }
     *
     *     BccEntryInput = [
     *         context: "Signature1",
     *         protected: bstr .cbor {
     *             1 : -8,                    // Algorithm : EdDSA
     *         },
     *         external_aad: bstr .size 0,
     *         payload: bstr .cbor BccPayload
     *     ]
     *
     *     DeviceInfo = {
     *         ? "brand" : tstr,
     *         ? "manufacturer" : tstr,
     *         ? "product" : tstr,
     *         ? "model" : tstr,
     *         ? "board" : tstr,
     *         ? "vb_state" : "green" / "yellow" / "orange",
     *         ? "bootloader_state" : "locked" / "unlocked",
     *         ? "os_version" : tstr,
     *         ? "system_patch_level" : uint,        // YYYYMMDD
     *         ? "boot_patch_level" : uint,          // YYYYMMDD
     *         ? "vendor_patch_level" : uint,        // YYYYMMDD
     *     }
     */
    byte[] protectedData;
}
