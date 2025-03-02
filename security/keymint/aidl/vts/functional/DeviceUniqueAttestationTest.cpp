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

#define LOG_TAG "keymint_1_attest_key_test"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <keymint_support/key_param_output.h>
#include <keymint_support/openssl_utils.h>

#include "KeyMintAidlTestBase.h"

namespace aidl::android::hardware::security::keymint::test {

class DeviceUniqueAttestationTest : public KeyMintAidlTestBase {
  protected:
    void CheckUniqueAttestationResults(const vector<uint8_t>& key_blob,
                                       const vector<KeyCharacteristics>& key_characteristics,
                                       const AuthorizationSet& hw_enforced, int key_size) {
        ASSERT_GT(cert_chain_.size(), 0);

        if (KeyMintAidlTestBase::dump_Attestations) {
            std::cout << bin2hex(cert_chain_[0].encodedCertificate) << std::endl;
        }

        ASSERT_GT(key_blob.size(), 0U);

        AuthorizationSet crypto_params = SecLevelAuthorizations(key_characteristics);

        EXPECT_TRUE(crypto_params.Contains(TAG_KEY_SIZE, key_size)) << "Key size missing";

        EXPECT_TRUE(ChainSignaturesAreValid(cert_chain_));
        ASSERT_GT(cert_chain_.size(), 0);

        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(key_characteristics);
        EXPECT_TRUE(verify_attestation_record("challenge", "foo", sw_enforced, hw_enforced,
                                              SecLevel(), cert_chain_[0].encodedCertificate));
    }
};

/*
 * DeviceUniqueAttestationTest.RsaNonStrongBoxUnimplemented
 *
 * Verifies that non strongbox implementations do not implement Rsa device unique
 * attestation.
 */
TEST_P(DeviceUniqueAttestationTest, RsaNonStrongBoxUnimplemented) {
    if (SecLevel() == SecurityLevel::STRONGBOX) return;

    vector<uint8_t> key_blob;
    vector<KeyCharacteristics> key_characteristics;

    // Check RSA implementation
    auto result = GenerateKey(AuthorizationSetBuilder()
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .RsaSigningKey(2048, 65537)
                                      .Digest(Digest::SHA_2_256)
                                      .Padding(PaddingMode::RSA_PKCS1_1_5_SIGN)
                                      .Authorization(TAG_INCLUDE_UNIQUE_ID)
                                      .AttestationChallenge("challenge")
                                      .AttestationApplicationId("foo")
                                      .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION),
                              &key_blob, &key_characteristics);

    ASSERT_EQ(result, ErrorCode::INVALID_ARGUMENT);
}

/*
 * DeviceUniqueAttestationTest.EcdsaNonStrongBoxUnimplemented
 *
 * Verifies that non strongbox implementations do not implement Ecdsa device unique
 * attestation.
 */
TEST_P(DeviceUniqueAttestationTest, EcdsaNonStrongBoxUnimplemented) {
    if (SecLevel() == SecurityLevel::STRONGBOX) return;

    vector<uint8_t> key_blob;
    vector<KeyCharacteristics> key_characteristics;

    // Check Ecdsa implementation
    auto result = GenerateKey(AuthorizationSetBuilder()
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .EcdsaSigningKey(EcCurve::P_256)
                                      .Digest(Digest::SHA_2_256)
                                      .Authorization(TAG_INCLUDE_UNIQUE_ID)
                                      .AttestationChallenge("challenge")
                                      .AttestationApplicationId("foo")
                                      .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION),
                              &key_blob, &key_characteristics);

    ASSERT_EQ(result, ErrorCode::INVALID_ARGUMENT);
}

/*
 * DeviceUniqueAttestationTest.RsaDeviceUniqueAttestation
 *
 * Verifies that strongbox implementations of Rsa implements device unique
 * attestation correctly, if implemented.
 */
TEST_P(DeviceUniqueAttestationTest, RsaDeviceUniqueAttestation) {
    if (SecLevel() != SecurityLevel::STRONGBOX) return;

    vector<uint8_t> key_blob;
    vector<KeyCharacteristics> key_characteristics;
    int key_size = 2048;

    auto result = GenerateKey(AuthorizationSetBuilder()
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .RsaSigningKey(key_size, 65537)
                                      .Digest(Digest::SHA_2_256)
                                      .Padding(PaddingMode::RSA_PKCS1_1_5_SIGN)
                                      .Authorization(TAG_INCLUDE_UNIQUE_ID)
                                      .AttestationChallenge("challenge")
                                      .AttestationApplicationId("foo")
                                      .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION),
                              &key_blob, &key_characteristics);

    // It is optional for Strong box to support DeviceUniqueAttestation.
    if (result == ErrorCode::CANNOT_ATTEST_IDS) return;

    ASSERT_EQ(ErrorCode::OK, result);

    AuthorizationSet hw_enforced = AuthorizationSetBuilder()
                                           .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION)
                                           .Authorization(TAG_NO_AUTH_REQUIRED)
                                           .RsaSigningKey(2048, 65537)
                                           .Digest(Digest::SHA_2_256)
                                           .Padding(PaddingMode::RSA_PKCS1_1_5_SIGN)
                                           .Authorization(TAG_ORIGIN, KeyOrigin::GENERATED)
                                           .Authorization(TAG_OS_VERSION, os_version())
                                           .Authorization(TAG_OS_PATCHLEVEL, os_patch_level());

    CheckUniqueAttestationResults(key_blob, key_characteristics, hw_enforced, key_size);
}

/*
 * DeviceUniqueAttestationTest.EcdsaDeviceUniqueAttestation
 *
 * Verifies that strongbox implementations of Rsa implements device unique
 * attestation correctly, if implemented.
 */
TEST_P(DeviceUniqueAttestationTest, EcdsaDeviceUniqueAttestation) {
    if (SecLevel() != SecurityLevel::STRONGBOX) return;

    vector<uint8_t> key_blob;
    vector<KeyCharacteristics> key_characteristics;
    int key_size = 256;

    auto result = GenerateKey(AuthorizationSetBuilder()
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .EcdsaSigningKey(256)
                                      .Digest(Digest::SHA_2_256)
                                      .Authorization(TAG_INCLUDE_UNIQUE_ID)
                                      .AttestationChallenge("challenge")
                                      .AttestationApplicationId("foo")
                                      .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION),
                              &key_blob, &key_characteristics);

    // It is optional for Strong box to support DeviceUniqueAttestation.
    if (result == ErrorCode::CANNOT_ATTEST_IDS) return;
    ASSERT_EQ(ErrorCode::OK, result);

    AuthorizationSet hw_enforced = AuthorizationSetBuilder()
                                           .Authorization(TAG_DEVICE_UNIQUE_ATTESTATION)
                                           .Authorization(TAG_NO_AUTH_REQUIRED)
                                           .EcdsaSigningKey(EcCurve::P_256)
                                           .Digest(Digest::SHA_2_256)
                                           .Authorization(TAG_EC_CURVE, EcCurve::P_256)
                                           .Authorization(TAG_ORIGIN, KeyOrigin::GENERATED)
                                           .Authorization(TAG_OS_VERSION, os_version())
                                           .Authorization(TAG_OS_PATCHLEVEL, os_patch_level());

    CheckUniqueAttestationResults(key_blob, key_characteristics, hw_enforced, key_size);
}

INSTANTIATE_KEYMINT_AIDL_TEST(DeviceUniqueAttestationTest);

}  // namespace aidl::android::hardware::security::keymint::test
