/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

#include "ClientEncryption.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>
#include <iostream>

ClientEncryption::ClientEncryption() : initialized(false)
{
}

ClientEncryption::~ClientEncryption()
{
    // clear sensitive data
    if (!key.empty())
    {
        OPENSSL_cleanse(key.data(), key.size());
    }
}

bool ClientEncryption::initialize(const std::string &password)
{
    // derive a 256 bit key from the password using sha-256
    key.resize(32); // 256 bits

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(key.data(), &sha256);

    initialized = true;
    return true;
}

void ClientEncryption::generateNonce(uint8_t *nonce)
{
    RAND_bytes(nonce, EncryptedPacket::NONCE_SIZE);
}

bool ClientEncryption::encrypt(const uint8_t *plaintext, size_t len, std::vector<uint8_t> &output)
{
    if (!initialized)
        return false;

    // create packet structure
    EncryptedPacket packet;
    packet.magic = EncryptedPacket::MAGIC;
    generateNonce(packet.nonce); // nonce :)

    size_t totalSize = sizeof(packet.magic) + EncryptedPacket::NONCE_SIZE +
                       EncryptedPacket::TAG_SIZE + len;
    output.resize(totalSize);

    // copy magic and nonce to output, or jail since its a nonce
    memcpy(output.data(), &packet.magic, sizeof(packet.magic));
    memcpy(output.data() + sizeof(packet.magic), packet.nonce, EncryptedPacket::NONCE_SIZE);

    // okay enough of the nonce jokes

    // set up AES GCM encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    int outlen;
    bool success = false;

    do
    {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
            break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, EncryptedPacket::NONCE_SIZE, NULL) != 1)
            break;

        if (EVP_EncryptInit_ex(ctx, NULL, NULL, key.data(), packet.nonce) != 1)
            break;

        uint8_t *ciphertextPtr = output.data() + sizeof(packet.magic) +
                                 EncryptedPacket::NONCE_SIZE + EncryptedPacket::TAG_SIZE;

        if (EVP_EncryptUpdate(ctx, ciphertextPtr, &outlen, plaintext, len) != 1)
            break;

        int finalLen;
        if (EVP_EncryptFinal_ex(ctx, ciphertextPtr + outlen, &finalLen) != 1)
            break;

        uint8_t *tagPtr = output.data() + sizeof(packet.magic) + EncryptedPacket::NONCE_SIZE;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, EncryptedPacket::TAG_SIZE, tagPtr) != 1)
            break;

        success = true;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);
    return success;
}

bool ClientEncryption::decrypt(const uint8_t *ciphertext, size_t len, std::vector<uint8_t> &output)
{
    if (!initialized)
        return false;

    size_t minSize = sizeof(uint32_t) + EncryptedPacket::NONCE_SIZE + EncryptedPacket::TAG_SIZE;
    if (len < minSize)
        return false;

    // check the magic number
    uint32_t magic;
    memcpy(&magic, ciphertext, sizeof(magic));
    if (magic != EncryptedPacket::MAGIC)
        return false;

    // extract
    const uint8_t *nonce = ciphertext + sizeof(magic);
    const uint8_t *tag = nonce + EncryptedPacket::NONCE_SIZE;
    const uint8_t *encryptedData = tag + EncryptedPacket::TAG_SIZE;
    size_t encryptedLen = len - minSize;

    output.resize(encryptedLen);

    // set up AES GCM encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    int outlen;
    bool success = false;

    do
    {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
            break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, EncryptedPacket::NONCE_SIZE, NULL) != 1)
            break;

        if (EVP_DecryptInit_ex(ctx, NULL, NULL, key.data(), nonce) != 1)
            break;

        // decrypt the data
        if (EVP_DecryptUpdate(ctx, output.data(), &outlen, encryptedData, encryptedLen) != 1)
            break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, EncryptedPacket::TAG_SIZE,
                                const_cast<uint8_t *>(tag)) != 1)
            break;

        // verify the decrypted message
        int finalLen;
        if (EVP_DecryptFinal_ex(ctx, output.data() + outlen, &finalLen) != 1)
            break;

        output.resize(outlen + finalLen);
        success = true;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);
    return success;
}

bool ClientEncryption::isEncryptedPacket(const uint8_t *data, size_t len)
{
    if (len < sizeof(uint32_t))
        return false;

    uint32_t magic;
    memcpy(&magic, data, sizeof(magic));
    return magic == EncryptedPacket::MAGIC;
}