#pragma once

#include <QString>

#include <string>
#include <unordered_map>
#include <vector>

namespace chatterino {

using AlphabetMap = std::unordered_map<char, std::string>;
using ReverseAlphabetMap = std::unordered_map<std::string, char>;

enum class EncryptionEncoding : int { Nothing = 0, Hebrew = 1, Chinese = 2 };
EncryptionEncoding parse_encryption_encoding(const QString &text);

class TextEncryption
{
public:
    TextEncryption() = default;
    ~TextEncryption() = default;

    std::string encrypt(const std::string &text, const std::string &password,
                        const EncryptionEncoding &encoding) const;

    std::string decrypt(const std::string &text, const std::string &password,
                        const EncryptionEncoding &encoding) const;

    EncryptionEncoding detect_encryption_encoding(
        const std::string &text) const;

private:
    std::vector<unsigned char> sha256_key(const std::string &password) const;

    std::string to_hex(const std::vector<unsigned char> &data) const;
    std::vector<unsigned char> from_hex(const std::string &hex) const;

    AlphabetMap get_alphabet(const EncryptionEncoding &encoding) const;
    ReverseAlphabetMap reverse_alphabet(const AlphabetMap &alphabet) const;

    const AlphabetMap HEBREW_ALPHABET = {
        {'0', "א"}, {'1', "ב"}, {'2', "ג"}, {'3', "ד"}, {'4', "ה"}, {'5', "ו"},
        {'6', "ז"}, {'7', "ח"}, {'8', "ט"}, {'9', "י"}, {'a', "כ"}, {'b', "ל"},
        {'c', "מ"}, {'d', "נ"}, {'e', "ס"}, {'f', "ע"}};

    const AlphabetMap CHINESE_ALPHABET = {
        {'0', "零"}, {'1', "一"}, {'2', "二"}, {'3', "三"},
        {'4', "四"}, {'5', "五"}, {'6', "六"}, {'7', "七"},
        {'8', "八"}, {'9', "九"}, {'a', "甲"}, {'b', "乙"},
        {'c', "丙"}, {'d', "丁"}, {'e', "戊"}, {'f', "己"}};
};
}  // namespace chatterino
