/*
 *  * DedCon RCON Client: Remote Access for Dedcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  * Copyright (C) 2025 Keiron Mcphee
 *  *
 *  */

#ifndef CLIENT_ENCRYPTION_H
#define CLIENT_ENCRYPTION_H

#include <string>
#include <vector>
#include <memory>
#include <openssl/evp.h>

struct EncryptedPacket
{
    static const uint32_t MAGIC = 0x52434F4E;                                   // RCON in hex
    static const size_t NONCE_SIZE = 12;
    static const size_t TAG_SIZE = 16;

    uint32_t magic;                                                             // magic number to identify encrypted packets
    uint8_t nonce[NONCE_SIZE];                                                  // random nonce for AES GCM, still cannot believe they decided to call it nonce. short for number once apparently
    uint8_t tag[TAG_SIZE];                                                      // authentication tag
    std::vector<uint8_t> ciphertext;

    bool isValid                                                                () const { return magic == MAGIC; }
};

class ClientEncryption
{
public:
    ClientEncryption                                                            ();
    ~ClientEncryption                                                           ();
    bool initialize                                                             (const std::string &password);
    bool encrypt                                                                (const uint8_t *plaintext, size_t len, std::vector<uint8_t> &output);
    bool decrypt                                                                (const uint8_t *ciphertext, size_t len, std::vector<uint8_t> &output);

    static bool isEncryptedPacket                                               (const uint8_t *data, size_t len);

private:
    std::vector<uint8_t> key;
    bool initialized;

    // helper to generate random number once
    void generateNonce                                                          (uint8_t *nonce);
};

#endif // CLIENT_ENCRYPTION_H