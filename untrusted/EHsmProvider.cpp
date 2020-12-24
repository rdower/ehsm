/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "EnclaveHelpers.h"
#include "enclave_hsm_u.h"

namespace EHsmProvider
{
    EH_RV Initialize()
    {
        SgxCrypto::EnclaveHelpers enclaveHelpers;

        if (!enclaveHelpers.isSgxEnclaveLoaded())
        {
			if (SGX_SUCCESS != enclaveHelpers.loadSgxEnclave())
            {
                return EHR_DEVICE_ERROR;
            }
        }

        return EHR_OK;
    }

    void Finalize()
    {
        SgxCrypto::EnclaveHelpers enclaveHelpers;

        if (enclaveHelpers.isSgxEnclaveLoaded())
        {
            enclaveHelpers.unloadSgxEnclave();
        }
    }

    EH_RV CreateKey(EH_MECHANISM_TYPE ulKeySpec, EH_KEY_ORIGIN eOrigin,
            EH_KEY_BLOB_PTR pKeyBlob)
    {
        sgx_status_t sgxStatus = SGX_ERROR_UNEXPECTED;
        sgx_status_t ret = SGX_ERROR_UNEXPECTED;
        SgxCrypto::EnclaveHelpers enclaveHelpers;

        if (eOrigin != EHO_INTERNAL_KEY || pKeyBlob == NULL) {
            return EHR_ARGUMENTS_BAD;
        }

        pKeyBlob->ulKeyType = ulKeySpec;

        switch (ulKeySpec) {
            case EHM_AES_GCM_128:
                if (pKeyBlob->pKeyData == NULL) {
                    ret = sgx_create_aes_key(enclaveHelpers.getSgxEnclaveId(),
                                             &sgxStatus,
                                             pKeyBlob->pKeyData,
                                             pKeyBlob->ulKeyLen,
                                             &(pKeyBlob->ulKeyLen));
                } else {
                    ret = sgx_create_aes_key(enclaveHelpers.getSgxEnclaveId(),
                                             &sgxStatus,
                                             pKeyBlob->pKeyData,
                                             pKeyBlob->ulKeyLen,
                                             NULL);
                }
                break;
            default:
                return EHR_MECHANISM_INVALID;
        }

        if (ret != SGX_SUCCESS || sgxStatus != SGX_SUCCESS)
            return EHR_FUNCTION_FAILED;
        else
            return EHR_OK;
    }

    EH_RV Encrypt(EH_MECHANISM_PTR pMechanism, EH_KEY_BLOB_PTR pKeyBlob,
            EH_BYTE_PTR pData, EH_ULONG ulDataLen,
            EH_BYTE_PTR pEncryptedData, EH_ULONG_PTR pulEncryptedDataLen)
    {
        sgx_status_t sgxStatus = SGX_ERROR_UNEXPECTED;
        sgx_status_t ret = SGX_ERROR_UNEXPECTED;
        SgxCrypto::EnclaveHelpers enclaveHelpers;

        if (pMechanism != NULL && pEncryptedData == NULL &&
                pulEncryptedDataLen != NULL) {
            return enclaveHelpers.getEncryptLen(pMechanism->mechanism,
                    ulDataLen, pulEncryptedDataLen);
        }

        if (pMechanism ==  NULL || pKeyBlob == NULL || pData == NULL ||
                pEncryptedData == NULL || pulEncryptedDataLen == NULL ||
                pMechanism->mechanism != pKeyBlob->ulKeyType) {
            return EHR_ARGUMENTS_BAD;
        }

        switch(pMechanism->mechanism) {
            case EHM_AES_GCM_128:
                // todo: refine later
                if (ulDataLen > EH_ENCRYPT_MAX_SIZE) {
                    return EHR_ARGUMENTS_BAD; 
                }

                if (pMechanism->ulParameterLen != sizeof(EH_GCM_PARAMS)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                ret = sgx_aes_encrypt(enclaveHelpers.getSgxEnclaveId(),
                                      &sgxStatus,
                                      EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD,
                                      EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen,
                                      pKeyBlob->pKeyData,
                                      pKeyBlob->ulKeyLen,
                                      pData,
                                      ulDataLen,
                                      pEncryptedData,
                                      *pulEncryptedDataLen);
                break;
            default:
                return EHR_MECHANISM_INVALID;
        }

        if (ret != SGX_SUCCESS || sgxStatus != SGX_SUCCESS)
            return EHR_FUNCTION_FAILED;
        else
            return EHR_OK;
    }

    EH_RV Decrypt(EH_MECHANISM_PTR pMechanism, EH_KEY_BLOB_PTR pKeyBlob,
            EH_BYTE_PTR pEncryptedData, EH_ULONG ulEncryptedDataLen,
            EH_BYTE_PTR pData, EH_ULONG_PTR pulDataLen)
    {
        sgx_status_t sgxStatus = SGX_ERROR_UNEXPECTED;
        sgx_status_t ret = SGX_ERROR_UNEXPECTED;
        SgxCrypto::EnclaveHelpers enclaveHelpers;

        if (pMechanism != NULL && pData == NULL && pulDataLen != NULL) {
            if (pMechanism->mechanism == EHM_AES_GCM_128) {
                if (ulEncryptedDataLen > EH_AES_GCM_IV_SIZE + EH_AES_GCM_MAC_SIZE) {
                    *pulDataLen = ulEncryptedDataLen - EH_AES_GCM_IV_SIZE -
                        EH_AES_GCM_MAC_SIZE;
                    return EHR_OK;
                }
            }
        }

        if (pMechanism ==  NULL || pKeyBlob == NULL || pData == NULL ||
                pEncryptedData == NULL || pulDataLen == NULL ||
                pMechanism->mechanism != pKeyBlob->ulKeyType) {
            return EHR_ARGUMENTS_BAD;
        }

        switch(pMechanism->mechanism) {
            case EHM_AES_GCM_128:
                if (pMechanism->ulParameterLen != sizeof(EH_GCM_PARAMS)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                ret = sgx_aes_decrypt(enclaveHelpers.getSgxEnclaveId(),
                                      &sgxStatus,
                                      EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD,
                                      EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen,
                                      pKeyBlob->pKeyData,
                                      pKeyBlob->ulKeyLen,
                                      pEncryptedData,
                                      ulEncryptedDataLen,
                                      pData,
                                      *pulDataLen);
                break;
            default:
                return EHR_MECHANISM_INVALID;
        }

        if (ret != SGX_SUCCESS || sgxStatus != SGX_SUCCESS)
            return EHR_FUNCTION_FAILED;
        else
            return EHR_OK;
    }

    EH_RV GenerateDataKey(EH_MECHANISM_PTR  pMechanism,
                          EH_KEY_BLOB_PTR   pMasterKeyBlob,
                          EH_BYTE_PTR       pPlainDataKey,
                          EH_ULONG          ulPlainDataKeyLen,
                          EH_BYTE_PTR       pEncryptedDataKey,
                          EH_ULONG_PTR      pulEncryptedDataKeyLen)
    {
        sgx_status_t sgxStatus = SGX_ERROR_UNEXPECTED;
        sgx_status_t ret = SGX_ERROR_UNEXPECTED;
        SgxCrypto::EnclaveHelpers enclaveHelpers;
        EH_BYTE_PTR pContext = NULL;
        EH_ULONG ulContextLen = 0;

        if (pMasterKeyBlob != NULL && pEncryptedDataKey == NULL &&
                pulEncryptedDataKeyLen != NULL) {
            return enclaveHelpers.getEncryptLen(pMasterKeyBlob->ulKeyType,
                    ulPlainDataKeyLen, pulEncryptedDataKeyLen);
        }

        if (ulPlainDataKeyLen > 1024 || ulPlainDataKeyLen == 0) {
            return EHR_ARGUMENTS_BAD;
        }

        if (pMechanism == NULL || pMasterKeyBlob ==  NULL ||
                pEncryptedDataKey == NULL || pulEncryptedDataKeyLen == NULL ||
                pMechanism->mechanism != pMasterKeyBlob->ulKeyType) {
            return EHR_ARGUMENTS_BAD;
        }

        switch(pMechanism->mechanism) {
            case EHM_AES_GCM_128:
                if (pMechanism->ulParameterLen != sizeof(EH_GCM_PARAMS)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                if ((0 == EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen) &&
                        (NULL != EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD)) {
                    return EHR_ARGUMENTS_BAD;
                }

                pContext = EH_GCM_PARAMS_PTR(pMechanism->pParameter)->pAAD;
                ulContextLen = EH_GCM_PARAMS_PTR(pMechanism->pParameter)->ulAADLen;

                break;
            default:
                return EHR_MECHANISM_INVALID;
        }

        ret = sgx_generate_datakey(enclaveHelpers.getSgxEnclaveId(),
                                   &sgxStatus,
                                   pMechanism->mechanism,
                                   pMasterKeyBlob->pKeyData,
                                   pMasterKeyBlob->ulKeyLen,
                                   pContext,
                                   ulContextLen,
                                   pPlainDataKey,
                                   ulPlainDataKeyLen,
                                   pEncryptedDataKey,
                                   *pulEncryptedDataKeyLen);

        if (ret != SGX_SUCCESS || sgxStatus != SGX_SUCCESS)
            return EHR_FUNCTION_FAILED;
        else
            return EHR_OK;
    }

    EH_RV GenerateDataKeyWithoutPlaintext(EH_MECHANISM_PTR  pMechanism,
                                          EH_KEY_BLOB_PTR   pMasterKeyBlob,
                                          EH_ULONG          ulPlainDataKeyLen,
                                          EH_BYTE_PTR       pEncryptedDataKey,
                                          EH_ULONG_PTR      pulEncryptedDataKeyLen)
    {
        return GenerateDataKey(pMechanism, pMasterKeyBlob, NULL, ulPlainDataKeyLen,
                pEncryptedDataKey, pulEncryptedDataKeyLen);
    }
}
