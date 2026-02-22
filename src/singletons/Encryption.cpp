#include "singletons/Encryption.hpp"

#include "common/QLogging.hpp"
#include "Encryption.hpp"
#include "singletons/Settings.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ios>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace chatterino {
EncryptionEncoding parse_encryption_encoding(const QString &text)
{
    if (text == "nothing")
    {
        return EncryptionEncoding::Nothing;
    }
    else if (text == "hebrew")
    {
        return EncryptionEncoding::Hebrew;
    }
    else
    {
        throw std::runtime_error("Unknown encryption encoding: " +
                                 text.toStdString());
    }
}

std::string TextEncryption::encrypt(const std::string &text,
                                    const std::string &password,
                                    const EncryptionEncoding &encoding) const
{
    auto key = this->sha256_key(password);
    std::vector<unsigned char> iv(16);
    if (RAND_bytes(iv.data(), iv.size()) != 1)
    {
        throw std::runtime_error("IV generation failed");
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw std::runtime_error("EVP context allocation failed");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(),
                           iv.data()) != 1)
    {
        throw std::runtime_error("EncryptInit failed");
    }

    std::vector<unsigned char> ciphertext(text.size() + 16);
    int len = 0, total = 0;

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char *>(text.data()),
                          text.size()) != 1)
    {
        throw std::runtime_error("EncryptUpdate failed");
    }

    total += len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &len) != 1)
        throw std::runtime_error("EncryptFinal failed");

    total += len;
    ciphertext.resize(total);
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> combined = iv;
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());

    std::string hex = this->to_hex(combined);
    std::string out;

    // encoding to alphabet
    auto alphabet = this->get_alphabet(encoding);
    for (char c : hex)
    {
        auto it = alphabet.find(c);
        if (it == alphabet.end())
        {
            throw std::runtime_error(
                "Invalid hex from mapping " +
                std::to_string(static_cast<int>(encoding)));
        }
        out += it->second;
    }

    if (getSettings()->randomSpaces)
    {
        std::string tmp;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        int symbol_size_bytes = 1;
        if (encoding == EncryptionEncoding::Hebrew)
        {
            symbol_size_bytes++;
        }

        for (int i = 0; i < out.size(); i += symbol_size_bytes)
        {
            tmp.append(out.substr(i, symbol_size_bytes));
            if (dis(gen) < 0.1)
            {
                tmp += ' ';
            }
        }

        out = tmp;
    }

    return out;
}

std::string TextEncryption::decrypt(const std::string &text,
                                    const std::string &password,
                                    const EncryptionEncoding &encoding) const
{
    auto key = this->sha256_key(password);
    auto alphabet = this->reverse_alphabet(this->get_alphabet(encoding));

    std::string spaceless_text = text;
    spaceless_text.erase(
        std::remove(spaceless_text.begin(), spaceless_text.end(), ' '),
        spaceless_text.end());

    // translating to hex
    std::string hex;
    for (int i = 0; i < spaceless_text.size();)
    {
        bool matched = false;

        for (const auto &kv : alphabet)
        {
            const std::string &symbol = kv.first;
            int len = symbol.size();

            if (i + len <= spaceless_text.size() &&
                spaceless_text.compare(i, len, symbol) == 0)
            {
                hex += kv.second;
                i += len;
                matched = true;
                break;
            }
        }

        if (!matched)
        {
            throw std::runtime_error("Text does not match selected alphabet");
        }
    }

    if (hex.size() % 2 != 0)
    {
        throw std::runtime_error("Decoded hex has invalid length");
    }

    // decryption
    auto raw = this->from_hex(hex);
    if (raw.size() < 16)
    {
        throw std::runtime_error("Ciphertext too short");
    }

    std::vector<unsigned char> iv(raw.begin(), raw.begin() + 16);
    std::vector<unsigned char> ciphertext(raw.begin() + 16, raw.end());

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP context allocation failed");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(),
                           iv.data()) != 1)
        throw std::runtime_error("DecryptInit failed");

    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0, total = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          ciphertext.size()) != 1)
        throw std::runtime_error("DecryptUpdate failed");

    total += len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Wrong password or corrupted data");
    }

    total += len;
    plaintext.resize(total);
    EVP_CIPHER_CTX_free(ctx);

    return std::string(plaintext.begin(), plaintext.end());
}

EncryptionEncoding TextEncryption::detect_encryption_encoding(
    const std::string &text) const
{
    return EncryptionEncoding::Hebrew;
}

std::vector<unsigned char> TextEncryption::sha256_key(
    const std::string &password) const
{
    if (password.empty())
        throw std::runtime_error("Empty password");
    std::vector<unsigned char> key(32);
    SHA256(reinterpret_cast<const unsigned char *>(password.data()),
           password.size(), key.data());
    return key;
}

std::string TextEncryption::to_hex(const std::vector<unsigned char> &data) const
{
    std::ostringstream oss;
    for (auto b : data)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

std::vector<unsigned char> TextEncryption::from_hex(
    const std::string &hex) const
{
    if (hex.size() % 2 != 0)
    {
        throw std::runtime_error("Invalid hex length");
    }

    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);

    for (int i = 0; i < hex.size(); i += 2)
    {
        std::string byte = hex.substr(i, 2);

        if (!std::isxdigit(byte[0]) || !std::isxdigit(byte[1]))
        {
            throw std::runtime_error("Invalid hex character");
        }
        out.push_back(
            static_cast<unsigned char>(std::stoul(byte, nullptr, 16)));
    }
    return out;
}

AlphabetMap TextEncryption::get_alphabet(
    const EncryptionEncoding &encoding) const
{
    switch (encoding)
    {
        case EncryptionEncoding::Hebrew:
            return HEBREW_ALPHABET;
        default:
            throw std::runtime_error("Unsupported encoding");
    }
}

ReverseAlphabetMap TextEncryption::reverse_alphabet(
    const AlphabetMap &alphabet) const
{
    ReverseAlphabetMap r;
    for (auto &p : alphabet)
        r[p.second] = p.first;
    return r;
}
}  // namespace chatterino
