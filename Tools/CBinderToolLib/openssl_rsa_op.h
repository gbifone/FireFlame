
#ifndef __OPENSSL_RSA_CODEC_H__    // rsa_op.h
#define __OPENSSL_RSA_CODEC_H__
#include <string>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

// ��Կָ��
const unsigned char PUBLIC_EXPONENT_HEX[] =
{ 0x01, 0x00, 0x01 };

// ˽Կָ��
const unsigned char PRIVATE_EXPONENT_HEX[] =
{ 0x68, 0x4D, 0x32, 0xAA, 0xE1, 0x3B, 0x28, 0xEA, 0x96, 0x48, 0x9A, 0x52, 0xCF, 0xD4, 0x11, \
0xBE, 0x8E, 0xC1, 0xC2, 0x36, 0xF2, 0x95, 0xB3, 0x66, 0x2E, 0x54, 0x49, 0xFD, 0xAE, 0xDC, \
0x1D, 0x8E, 0x86, 0xAA, 0xAD, 0x60, 0x5E, 0x82, 0xCD, 0x99, 0xA9, 0x96, 0x64, 0xB0, 0x70, \
0xA0, 0xC5, 0x3A, 0x78, 0x8B, 0x5F, 0x85, 0x7A, 0x31, 0x21, 0x95, 0xDD, 0xDC, 0x99, 0x0E, \
0x88, 0x4E, 0xA1, 0x3D, 0x8B, 0xF8, 0x58, 0xA1, 0x7C, 0xE8, 0x8C, 0x37, 0xE1, 0x1D, 0x59, \
0x76, 0x81, 0x48, 0xFC, 0xF0, 0x1C, 0x37, 0x5A, 0x39, 0x23, 0x05, 0xAB, 0xC1, 0x75, 0xC8, \
0x7F, 0x7A, 0xA6, 0xB9, 0x25, 0x9D, 0x36, 0xE7, 0x9E, 0xC5, 0xCE, 0x32, 0x45, 0x34, 0xE2, \
0xEC, 0xDF, 0xB1, 0xD1, 0x4D, 0xC9, 0x31, 0x55, 0xBA, 0x14, 0xB1, 0xD1, 0x09, 0x22, 0x69, \
0xCF, 0x09, 0xB9, 0xF6, 0xB6, 0x68, 0xA1, 0x49 };
// ģ��
const unsigned char MODULES_HEX[] =
{ 0xD7, 0x42, 0xCC, 0x97, 0x4D, 0x35, 0x1A, 0x8F, 0xB3, 0xAA, 0x42, 0xAA, 0x6D, 0x10, 0xEB, \
0x09, 0x58, 0xFA, 0xD2, 0xFB, 0x21, 0x0C, 0xDB, 0xBA, 0xB7, 0x22, 0x45, 0xE0, 0xF8, 0x1F, \
0x40, 0x26, 0xFD, 0x00, 0xAF, 0x83, 0x1B, 0x5C, 0xE5, 0x68, 0x7B, 0x3F, 0x81, 0x21, 0x9E, \
0xB4, 0x6B, 0x91, 0xCB, 0x5F, 0x2F, 0x6F, 0x18, 0xA6, 0x4B, 0xA0, 0x83, 0x33, 0x41, 0x7A, \
0x75, 0xE3, 0x4B, 0xF1, 0x23, 0xCC, 0xA5, 0x76, 0xD0, 0x58, 0x8F, 0x87, 0xE6, 0x4C, 0x66, \
0xB7, 0x83, 0x29, 0x16, 0xAE, 0x95, 0xE3, 0x76, 0x40, 0x0D, 0x54, 0xB8, 0x87, 0x0E, 0x8D, \
0x66, 0x0E, 0x0E, 0x1D, 0xC4, 0x16, 0xFD, 0x4F, 0xFA, 0xC4, 0xB9, 0x89, 0x5D, 0x01, 0x2D, \
0x86, 0x25, 0x44, 0x4B, 0x61, 0x31, 0xE2, 0xBD, 0x9A, 0xCD, 0x58, 0xE6, 0x6A, 0x94, 0xEC, \
0x94, 0x77, 0x64, 0x50, 0x8C, 0x04, 0xE8, 0x3F };

#define RSA_KEY_LENGTH 1024
static const char rnd_seed[] = "string to make the random number generator initialized";
class rsa_op
{
public:
    rsa_op();
    ~rsa_op();

    // init params
    int set_params(const unsigned char *pub_expd = PUBLIC_EXPONENT_HEX, int pub_expd_len = 3,
        const unsigned char *pri_expd = PRIVATE_EXPONENT_HEX, int pri_expd_len = 128,
        const unsigned char *module = MODULES_HEX, int module_len = 128);

    // open keys
    int open_prikey_pubkey();
    int open_prikey();
    int open_pubkey();

    int open_pubkey_from_string(const std::string& key);
    int get_pubkey_block_len() { return RSA_size(_pub_key); }

    // private key to encryption and public key to decryption
    int prikey_encrypt(const unsigned char *in, int in_len,
        unsigned char **out, int &out_len);
    int pubkey_decrypt(const unsigned char *in, int in_len,
        unsigned char **out, int &out_len);
    // public key to encryption and private key to decryption
    int pubkey_encrypt(const unsigned char *in, int in_len,
        unsigned char **out, int &out_len);
    int prikey_decrypt(const unsigned char *in, int in_len,
        unsigned char **out, int &out_len);

    int close_key();
protected:
    void free_res();

private:
    RSA* _pub_key;
    RSA* _pri_key;

    unsigned char *_pub_expd;
    unsigned char *_pri_expd;
    unsigned char *_module;

    int _pub_expd_len;
    int _pri_expd_len;
    int _module_len;
};

#endif
