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

#include <keymint_support/key_param_output.h>
#include <keymint_support/openssl_utils.h>

#include "KeyMintAidlTestBase.h"

namespace aidl::android::hardware::security::keymint::test {

namespace {

bool IsSelfSigned(const vector<Certificate>& chain) {
    if (chain.size() != 1) return false;
    return ChainSignaturesAreValid(chain);
}

}  // namespace

using AttestKeyTest = KeyMintAidlTestBase;

/*
 * AttestKeyTest.AllRsaSizes
 *
 * This test creates self signed RSA attestation keys of various sizes, and verify they can be
 * used to sign other RSA and EC keys.
 */
TEST_P(AttestKeyTest, AllRsaSizes) {
    for (auto size : ValidKeySizes(Algorithm::RSA)) {
        /*
         * Create attestation key.
         */
        AttestationKey attest_key;
        vector<KeyCharacteristics> attest_key_characteristics;
        vector<Certificate> attest_key_cert_chain;
        ASSERT_EQ(ErrorCode::OK, GenerateKey(AuthorizationSetBuilder()
                                                     .RsaSigningKey(size, 65537)
                                                     .AttestKey()
                                                     .SetDefaultValidity(),
                                             {} /* attestation signing key */, &attest_key.keyBlob,
                                             &attest_key_characteristics, &attest_key_cert_chain));

        ASSERT_GT(attest_key_cert_chain.size(), 0);
        EXPECT_EQ(attest_key_cert_chain.size(), 1);
        EXPECT_TRUE(IsSelfSigned(attest_key_cert_chain)) << "Failed on size " << size;

        /*
         * Use attestation key to sign RSA signing key
         */
        attest_key.issuerSubjectName = make_name_from_str("Android Keystore Key");
        vector<uint8_t> attested_key_blob;
        vector<KeyCharacteristics> attested_key_characteristics;
        vector<Certificate> attested_key_cert_chain;
        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .RsaSigningKey(2048, 65537)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attested_key_blob);

        AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              attested_key_cert_chain[0].encodedCertificate));

        // Attestation by itself is not valid (last entry is not self-signed).
        EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
        attested_key_cert_chain.push_back(attest_key_cert_chain[0]);
        EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));
        EXPECT_EQ(attested_key_cert_chain.size(), 2);

        /*
         * Use attestation key to sign RSA decryption key
         */
        attested_key_characteristics.resize(0);
        attested_key_cert_chain.resize(0);
        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .RsaEncryptionKey(2048, 65537)
                                      .Digest(Digest::NONE)
                                      .Padding(PaddingMode::NONE)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationChallenge("foo2")
                                      .AttestationApplicationId("bar2")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attested_key_blob);

        hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo2", "bar2", sw_enforced, hw_enforced, SecLevel(),
                                              attested_key_cert_chain[0].encodedCertificate));

        // Attestation by itself is not valid (last entry is not self-signed).
        EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
        attested_key_cert_chain.push_back(attest_key_cert_chain[0]);
        EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));
        EXPECT_EQ(attested_key_cert_chain.size(), 2);

        /*
         * Use attestation key to sign EC key. Specify a CREATION_DATETIME for this one.
         */
        attested_key_characteristics.resize(0);
        attested_key_cert_chain.resize(0);
        uint64_t timestamp = 1619621648000;
        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .EcdsaSigningKey(EcCurve::P_256)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .Authorization(TAG_CREATION_DATETIME, timestamp)
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attested_key_blob);
        CheckedDeleteKey(&attest_key.keyBlob);

        hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        // The client-specified CREATION_DATETIME should be in sw_enforced.
        // Its presence will also trigger verify_attestation_record() to check that it
        // is in the attestation extension with a matching value.
        EXPECT_TRUE(sw_enforced.Contains(TAG_CREATION_DATETIME, timestamp))
                << "expected CREATION_TIMESTAMP in sw_enforced:" << sw_enforced
                << " not in hw_enforced:" << hw_enforced;
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              attested_key_cert_chain[0].encodedCertificate));

        // Attestation by itself is not valid (last entry is not self-signed).
        EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
        attested_key_cert_chain.push_back(attest_key_cert_chain[0]);
        EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Bail early if anything failed.
        if (HasFailure()) return;
    }
}

/*
 * AttestKeyTest.RsaAttestedAttestKeys
 *
 * This test creates an RSA attestation key signed by factory keys, and varifies it can be
 * used to sign other RSA and EC keys.
 */
TEST_P(AttestKeyTest, RsaAttestedAttestKeys) {
    auto challenge = "hello";
    auto app_id = "foo";

    auto subject = "cert subj 2";
    vector<uint8_t> subject_der(make_name_from_str(subject));

    uint64_t serial_int = 66;
    vector<uint8_t> serial_blob(build_serial_blob(serial_int));

    /*
     * Create attestation key.
     */
    AttestationKey attest_key;
    vector<KeyCharacteristics> attest_key_characteristics;
    vector<Certificate> attest_key_cert_chain;
    ASSERT_EQ(ErrorCode::OK,
              GenerateKey(AuthorizationSetBuilder()
                                  .RsaSigningKey(2048, 65537)
                                  .AttestKey()
                                  .AttestationChallenge(challenge)
                                  .AttestationApplicationId(app_id)
                                  .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob)
                                  .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der)
                                  .Authorization(TAG_NO_AUTH_REQUIRED)
                                  .SetDefaultValidity(),
                          {} /* attestation signing key */, &attest_key.keyBlob,
                          &attest_key_characteristics, &attest_key_cert_chain));

    EXPECT_GT(attest_key_cert_chain.size(), 1);
    verify_subject_and_serial(attest_key_cert_chain[0], serial_int, subject, false);
    EXPECT_TRUE(ChainSignaturesAreValid(attest_key_cert_chain));

    AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attest_key_characteristics);
    AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attest_key_characteristics);
    EXPECT_TRUE(verify_attestation_record(challenge, app_id,  //
                                          sw_enforced, hw_enforced, SecLevel(),
                                          attest_key_cert_chain[0].encodedCertificate));

    /*
     * Use attestation key to sign RSA key
     */
    attest_key.issuerSubjectName = subject_der;
    vector<uint8_t> attested_key_blob;
    vector<KeyCharacteristics> attested_key_characteristics;
    vector<Certificate> attested_key_cert_chain;

    auto subject2 = "cert subject";
    vector<uint8_t> subject_der2(make_name_from_str(subject2));

    uint64_t serial_int2 = 987;
    vector<uint8_t> serial_blob2(build_serial_blob(serial_int2));

    EXPECT_EQ(ErrorCode::OK,
              GenerateKey(AuthorizationSetBuilder()
                                  .RsaSigningKey(2048, 65537)
                                  .Authorization(TAG_NO_AUTH_REQUIRED)
                                  .AttestationChallenge("foo")
                                  .AttestationApplicationId("bar")
                                  .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob2)
                                  .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der2)
                                  .SetDefaultValidity(),
                          attest_key, &attested_key_blob, &attested_key_characteristics,
                          &attested_key_cert_chain));

    CheckedDeleteKey(&attested_key_blob);
    CheckedDeleteKey(&attest_key.keyBlob);

    AuthorizationSet hw_enforced2 = HwEnforcedAuthorizations(attested_key_characteristics);
    AuthorizationSet sw_enforced2 = SwEnforcedAuthorizations(attested_key_characteristics);
    EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced2, hw_enforced2, SecLevel(),
                                          attested_key_cert_chain[0].encodedCertificate));

    // Attestation by itself is not valid (last entry is not self-signed).
    EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

    // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
    attested_key_cert_chain.insert(attested_key_cert_chain.end(), attest_key_cert_chain.begin(),
                                   attest_key_cert_chain.end());

    EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));
    EXPECT_GT(attested_key_cert_chain.size(), 2);
    verify_subject_and_serial(attested_key_cert_chain[0], serial_int2, subject2, false);
}

/*
 * AttestKeyTest.RsaAttestKeyChaining
 *
 * This test creates a chain of multiple RSA attest keys, each used to sign the next attest key,
 * with the last attest key signed by the factory chain.
 */
TEST_P(AttestKeyTest, RsaAttestKeyChaining) {
    const int chain_size = 6;
    vector<vector<uint8_t>> key_blob_list(chain_size);
    vector<vector<Certificate>> cert_chain_list(chain_size);

    for (int i = 0; i < chain_size; i++) {
        string sub = "attest key chaining ";
        char index = '1' + i;
        string subject = sub + index;
        vector<uint8_t> subject_der(make_name_from_str(subject));

        uint64_t serial_int = 7000 + i;
        vector<uint8_t> serial_blob(build_serial_blob(serial_int));

        vector<KeyCharacteristics> attested_key_characteristics;
        AttestationKey attest_key;
        optional<AttestationKey> attest_key_opt;

        if (i > 0) {
            attest_key.issuerSubjectName = make_name_from_str(sub + (char)(index - 1));
            attest_key.keyBlob = key_blob_list[i - 1];
            attest_key_opt = attest_key;
        }

        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .RsaSigningKey(2048, 65537)
                                      .AttestKey()
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob)
                                      .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der)
                                      .SetDefaultValidity(),
                              attest_key_opt, &key_blob_list[i], &attested_key_characteristics,
                              &cert_chain_list[i]));

        AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              cert_chain_list[i][0].encodedCertificate));

        if (i > 0) {
            /*
             * The first key is attestated with factory chain, but all the rest of the keys are
             * not supposed to be returned in attestation certificate chains.
             */
            EXPECT_FALSE(ChainSignaturesAreValid(cert_chain_list[i]));

            // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
            cert_chain_list[i].insert(cert_chain_list[i].end(),        //
                                      cert_chain_list[i - 1].begin(),  //
                                      cert_chain_list[i - 1].end());
        }

        EXPECT_TRUE(ChainSignaturesAreValid(cert_chain_list[i]));
        EXPECT_GT(cert_chain_list[i].size(), i + 1);
        verify_subject_and_serial(cert_chain_list[i][0], serial_int, subject, false);
    }

    for (int i = 0; i < chain_size; i++) {
        CheckedDeleteKey(&key_blob_list[i]);
    }
}

/*
 * AttestKeyTest.EcAttestKeyChaining
 *
 * This test creates a chain of multiple Ec attest keys, each used to sign the next attest key,
 * with the last attest key signed by the factory chain.
 */
TEST_P(AttestKeyTest, EcAttestKeyChaining) {
    const int chain_size = 6;
    vector<vector<uint8_t>> key_blob_list(chain_size);
    vector<vector<Certificate>> cert_chain_list(chain_size);

    for (int i = 0; i < chain_size; i++) {
        string sub = "Ec attest key chaining ";
        char index = '1' + i;
        string subject = sub + index;
        vector<uint8_t> subject_der(make_name_from_str(subject));

        uint64_t serial_int = 800000 + i;
        vector<uint8_t> serial_blob(build_serial_blob(serial_int));

        vector<KeyCharacteristics> attested_key_characteristics;
        AttestationKey attest_key;
        optional<AttestationKey> attest_key_opt;

        if (i > 0) {
            attest_key.issuerSubjectName = make_name_from_str(sub + (char)(index - 1));
            attest_key.keyBlob = key_blob_list[i - 1];
            attest_key_opt = attest_key;
        }

        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .EcdsaSigningKey(EcCurve::P_256)
                                      .AttestKey()
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob)
                                      .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .SetDefaultValidity(),
                              attest_key_opt, &key_blob_list[i], &attested_key_characteristics,
                              &cert_chain_list[i]));

        AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              cert_chain_list[i][0].encodedCertificate));

        if (i > 0) {
            /*
             * The first key is attestated with factory chain, but all the rest of the keys are
             * not supposed to be returned in attestation certificate chains.
             */
            EXPECT_FALSE(ChainSignaturesAreValid(cert_chain_list[i]));

            // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
            cert_chain_list[i].insert(cert_chain_list[i].end(),        //
                                      cert_chain_list[i - 1].begin(),  //
                                      cert_chain_list[i - 1].end());
        }

        EXPECT_TRUE(ChainSignaturesAreValid(cert_chain_list[i]));
        EXPECT_GT(cert_chain_list[i].size(), i + 1);
        verify_subject_and_serial(cert_chain_list[i][0], serial_int, subject, false);
    }

    for (int i = 0; i < chain_size; i++) {
        CheckedDeleteKey(&key_blob_list[i]);
    }
}

/*
 * AttestKeyTest.AlternateAttestKeyChaining
 *
 * This test creates a chain of multiple attest keys, in the order Ec - RSA - Ec - RSA ....
 * Each attest key is used to sign the next attest key, with the last attest key signed by
 * the factory chain. This is to verify different algorithms of attest keys can
 * cross sign each other and be chained together.
 */
TEST_P(AttestKeyTest, AlternateAttestKeyChaining) {
    const int chain_size = 6;
    vector<vector<uint8_t>> key_blob_list(chain_size);
    vector<vector<Certificate>> cert_chain_list(chain_size);

    for (int i = 0; i < chain_size; i++) {
        string sub = "Alt attest key chaining ";
        char index = '1' + i;
        string subject = sub + index;
        vector<uint8_t> subject_der(make_name_from_str(subject));

        uint64_t serial_int = 90000000 + i;
        vector<uint8_t> serial_blob(build_serial_blob(serial_int));

        vector<KeyCharacteristics> attested_key_characteristics;
        AttestationKey attest_key;
        optional<AttestationKey> attest_key_opt;

        if (i > 0) {
            attest_key.issuerSubjectName = make_name_from_str(sub + (char)(index - 1));
            attest_key.keyBlob = key_blob_list[i - 1];
            attest_key_opt = attest_key;
        }

        if ((i & 0x1) == 1) {
            EXPECT_EQ(ErrorCode::OK,
                      GenerateKey(AuthorizationSetBuilder()
                                          .EcdsaSigningKey(EcCurve::P_256)
                                          .AttestKey()
                                          .AttestationChallenge("foo")
                                          .AttestationApplicationId("bar")
                                          .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob)
                                          .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der)
                                          .Authorization(TAG_NO_AUTH_REQUIRED)
                                          .SetDefaultValidity(),
                                  attest_key_opt, &key_blob_list[i], &attested_key_characteristics,
                                  &cert_chain_list[i]));
        } else {
            EXPECT_EQ(ErrorCode::OK,
                      GenerateKey(AuthorizationSetBuilder()
                                          .RsaSigningKey(2048, 65537)
                                          .AttestKey()
                                          .AttestationChallenge("foo")
                                          .AttestationApplicationId("bar")
                                          .Authorization(TAG_CERTIFICATE_SERIAL, serial_blob)
                                          .Authorization(TAG_CERTIFICATE_SUBJECT, subject_der)
                                          .Authorization(TAG_NO_AUTH_REQUIRED)
                                          .SetDefaultValidity(),
                                  attest_key_opt, &key_blob_list[i], &attested_key_characteristics,
                                  &cert_chain_list[i]));
        }

        AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              cert_chain_list[i][0].encodedCertificate));

        if (i > 0) {
            /*
             * The first key is attestated with factory chain, but all the rest of the keys are
             * not supposed to be returned in attestation certificate chains.
             */
            EXPECT_FALSE(ChainSignaturesAreValid(cert_chain_list[i]));

            // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
            cert_chain_list[i].insert(cert_chain_list[i].end(),        //
                                      cert_chain_list[i - 1].begin(),  //
                                      cert_chain_list[i - 1].end());
        }

        EXPECT_TRUE(ChainSignaturesAreValid(cert_chain_list[i]));
        EXPECT_GT(cert_chain_list[i].size(), i + 1);
        verify_subject_and_serial(cert_chain_list[i][0], serial_int, subject, false);
    }

    for (int i = 0; i < chain_size; i++) {
        CheckedDeleteKey(&key_blob_list[i]);
    }
}

TEST_P(AttestKeyTest, MissingChallenge) {
    for (auto size : ValidKeySizes(Algorithm::RSA)) {
        /*
         * Create attestation key.
         */
        AttestationKey attest_key;
        vector<KeyCharacteristics> attest_key_characteristics;
        vector<Certificate> attest_key_cert_chain;
        ASSERT_EQ(ErrorCode::OK, GenerateKey(AuthorizationSetBuilder()
                                                     .RsaSigningKey(size, 65537)
                                                     .AttestKey()
                                                     .SetDefaultValidity(),
                                             {} /* attestation signing key */, &attest_key.keyBlob,
                                             &attest_key_characteristics, &attest_key_cert_chain));

        EXPECT_EQ(attest_key_cert_chain.size(), 1);
        EXPECT_TRUE(IsSelfSigned(attest_key_cert_chain)) << "Failed on size " << size;

        /*
         * Use attestation key to sign RSA / ECDSA key but forget to provide a challenge
         */
        attest_key.issuerSubjectName = make_name_from_str("Android Keystore Key");
        vector<uint8_t> attested_key_blob;
        vector<KeyCharacteristics> attested_key_characteristics;
        vector<Certificate> attested_key_cert_chain;
        EXPECT_EQ(ErrorCode::ATTESTATION_CHALLENGE_MISSING,
                  GenerateKey(AuthorizationSetBuilder()
                                      .RsaSigningKey(2048, 65537)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationApplicationId("bar")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        EXPECT_EQ(ErrorCode::ATTESTATION_CHALLENGE_MISSING,
                  GenerateKey(AuthorizationSetBuilder()
                                      .EcdsaSigningKey(EcCurve::P_256)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationApplicationId("bar")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attest_key.keyBlob);
    }
}

TEST_P(AttestKeyTest, AllEcCurves) {
    for (auto curve : ValidCurves()) {
        /*
         * Create attestation key.
         */
        AttestationKey attest_key;
        vector<KeyCharacteristics> attest_key_characteristics;
        vector<Certificate> attest_key_cert_chain;
        ASSERT_EQ(ErrorCode::OK, GenerateKey(AuthorizationSetBuilder()
                                                     .EcdsaSigningKey(curve)
                                                     .AttestKey()
                                                     .SetDefaultValidity(),
                                             {} /* attestation siging key */, &attest_key.keyBlob,
                                             &attest_key_characteristics, &attest_key_cert_chain));

        ASSERT_GT(attest_key_cert_chain.size(), 0);
        EXPECT_EQ(attest_key_cert_chain.size(), 1);
        EXPECT_TRUE(IsSelfSigned(attest_key_cert_chain)) << "Failed on curve " << curve;

        /*
         * Use attestation key to sign RSA key
         */
        attest_key.issuerSubjectName = make_name_from_str("Android Keystore Key");
        vector<uint8_t> attested_key_blob;
        vector<KeyCharacteristics> attested_key_characteristics;
        vector<Certificate> attested_key_cert_chain;
        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .RsaSigningKey(2048, 65537)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attested_key_blob);

        AuthorizationSet hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        AuthorizationSet sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              attested_key_cert_chain[0].encodedCertificate));

        // Attestation by itself is not valid (last entry is not self-signed).
        EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
        if (attest_key_cert_chain.size() > 0) {
            attested_key_cert_chain.push_back(attest_key_cert_chain[0]);
        }
        EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));

        /*
         * Use attestation key to sign EC key
         */
        EXPECT_EQ(ErrorCode::OK,
                  GenerateKey(AuthorizationSetBuilder()
                                      .EcdsaSigningKey(EcCurve::P_256)
                                      .Authorization(TAG_NO_AUTH_REQUIRED)
                                      .AttestationChallenge("foo")
                                      .AttestationApplicationId("bar")
                                      .SetDefaultValidity(),
                              attest_key, &attested_key_blob, &attested_key_characteristics,
                              &attested_key_cert_chain));

        CheckedDeleteKey(&attested_key_blob);
        CheckedDeleteKey(&attest_key.keyBlob);

        hw_enforced = HwEnforcedAuthorizations(attested_key_characteristics);
        sw_enforced = SwEnforcedAuthorizations(attested_key_characteristics);
        EXPECT_TRUE(verify_attestation_record("foo", "bar", sw_enforced, hw_enforced, SecLevel(),
                                              attested_key_cert_chain[0].encodedCertificate));

        // Attestation by itself is not valid (last entry is not self-signed).
        EXPECT_FALSE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Appending the attest_key chain to the attested_key_chain should yield a valid chain.
        if (attest_key_cert_chain.size() > 0) {
            attested_key_cert_chain.push_back(attest_key_cert_chain[0]);
        }
        EXPECT_TRUE(ChainSignaturesAreValid(attested_key_cert_chain));

        // Bail early if anything failed.
        if (HasFailure()) return;
    }
}

TEST_P(AttestKeyTest, AttestWithNonAttestKey) {
    // Create non-attestation key.
    AttestationKey non_attest_key;
    vector<KeyCharacteristics> non_attest_key_characteristics;
    vector<Certificate> non_attest_key_cert_chain;
    ASSERT_EQ(
            ErrorCode::OK,
            GenerateKey(
                    AuthorizationSetBuilder().EcdsaSigningKey(EcCurve::P_256).SetDefaultValidity(),
                    {} /* attestation siging key */, &non_attest_key.keyBlob,
                    &non_attest_key_characteristics, &non_attest_key_cert_chain));

    ASSERT_GT(non_attest_key_cert_chain.size(), 0);
    EXPECT_EQ(non_attest_key_cert_chain.size(), 1);
    EXPECT_TRUE(IsSelfSigned(non_attest_key_cert_chain));

    // Attempt to sign attestation with non-attest key.
    vector<uint8_t> attested_key_blob;
    vector<KeyCharacteristics> attested_key_characteristics;
    vector<Certificate> attested_key_cert_chain;
    EXPECT_EQ(ErrorCode::INCOMPATIBLE_PURPOSE,
              GenerateKey(AuthorizationSetBuilder()
                                  .EcdsaSigningKey(EcCurve::P_256)
                                  .Authorization(TAG_NO_AUTH_REQUIRED)
                                  .AttestationChallenge("foo")
                                  .AttestationApplicationId("bar")
                                  .SetDefaultValidity(),
                          non_attest_key, &attested_key_blob, &attested_key_characteristics,
                          &attested_key_cert_chain));
}

INSTANTIATE_KEYMINT_AIDL_TEST(AttestKeyTest);

}  // namespace aidl::android::hardware::security::keymint::test
