/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

// Paying homage to berts FakeEncryption file 
// this file contains real encryption for network packets using OpenSSL
// the plain udp implementation worked just fine and would probably have no issues
// however it makes me uneasy knowing that the password for the server is being
// transmitted in plain text through the internet, so here is the solution to that.

#include "RealEncryption.h"
#include "Main/Log.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>

std::unique_ptr<RealEncryption> g_serverEncryption;

bool initServerEncryption(const std::string &password)
{
    g_serverEncryption = std::make_unique<RealEncryption>();
    return g_serverEncryption->initialize(password);
}

RealEncryption::RealEncryption() : initialized(false)
{
}

RealEncryption::~RealEncryption()
{
    if (!key.empty())
    {
        OPENSSL_cleanse(key.data(), key.size());
    }
}

bool RealEncryption::initialize(const std::string &password)
{
    // derive a 256 bit key from the password using sha-256
    key.resize(32); // 256 bits

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(key.data(), &sha256);

    initialized = true;
    Log::Out() << "Server encryption initialized\n";
    return true;
}

void RealEncryption::generateNonce(uint8_t *nonce)
{
    RAND_bytes(nonce, ServerEncryptedPacket::NONCE_SIZE);
}

bool RealEncryption::encrypt(const uint8_t *plaintext, size_t len, std::vector<uint8_t> &output)
{
    if (!initialized)
        return false;

    ServerEncryptedPacket packet;
    packet.magic = ServerEncryptedPacket::MAGIC;
    generateNonce(packet.nonce); // nonce :)

    size_t totalSize = sizeof(packet.magic) + ServerEncryptedPacket::NONCE_SIZE +
                       ServerEncryptedPacket::TAG_SIZE + len;
    output.resize(totalSize);

    // copy magic and nonce to output, or jail since its a nonce
    memcpy(output.data(), &packet.magic, sizeof(packet.magic));
    memcpy(output.data() + sizeof(packet.magic), packet.nonce, ServerEncryptedPacket::NONCE_SIZE);

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

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ServerEncryptedPacket::NONCE_SIZE, NULL) != 1)
            break;

        if (EVP_EncryptInit_ex(ctx, NULL, NULL, key.data(), packet.nonce) != 1)
            break;

        uint8_t *ciphertextPtr = output.data() + sizeof(packet.magic) +
                                 ServerEncryptedPacket::NONCE_SIZE + ServerEncryptedPacket::TAG_SIZE;

        if (EVP_EncryptUpdate(ctx, ciphertextPtr, &outlen, plaintext, len) != 1)
            break;

        int finalLen;
        if (EVP_EncryptFinal_ex(ctx, ciphertextPtr + outlen, &finalLen) != 1)
            break;

        uint8_t *tagPtr = output.data() + sizeof(packet.magic) + ServerEncryptedPacket::NONCE_SIZE;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, ServerEncryptedPacket::TAG_SIZE, tagPtr) != 1)
            break;

        success = true;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);
    return success;
}

bool RealEncryption::decrypt(const uint8_t *ciphertext, size_t len, std::vector<uint8_t> &output)
{
    if (!initialized)
        return false;

    size_t minSize = sizeof(uint32_t) + ServerEncryptedPacket::NONCE_SIZE + ServerEncryptedPacket::TAG_SIZE;
    if (len < minSize)
        return false;

    uint32_t magic;
    memcpy(&magic, ciphertext, sizeof(magic));
    if (magic != ServerEncryptedPacket::MAGIC)
        return false;

    const uint8_t *nonce = ciphertext + sizeof(magic);
    const uint8_t *tag = nonce + ServerEncryptedPacket::NONCE_SIZE;
    const uint8_t *encryptedData = tag + ServerEncryptedPacket::TAG_SIZE;
    size_t encryptedLen = len - minSize;

    output.resize(encryptedLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    int outlen;
    bool success = false;

    do
    {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
            break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ServerEncryptedPacket::NONCE_SIZE, NULL) != 1)
            break;

        if (EVP_DecryptInit_ex(ctx, NULL, NULL, key.data(), nonce) != 1)
            break;

        if (EVP_DecryptUpdate(ctx, output.data(), &outlen, encryptedData, encryptedLen) != 1)
            break;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, ServerEncryptedPacket::TAG_SIZE,
                                const_cast<uint8_t *>(tag)) != 1)
            break;

        int finalLen;
        if (EVP_DecryptFinal_ex(ctx, output.data() + outlen, &finalLen) != 1)
            break;

        output.resize(outlen + finalLen);
        success = true;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);
    return success;
}

bool RealEncryption::isEncryptedPacket(const uint8_t *data, size_t len)
{
    if (len < sizeof(uint32_t))
        return false;

    uint32_t magic;
    memcpy(&magic, data, sizeof(magic));
    return magic == ServerEncryptedPacket::MAGIC;
}